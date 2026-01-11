#include "exam_handler.h"
#include "client_manager.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Helper function to shuffle questions randomly
static void shuffle_questions(cJSON* questions, int count)
{
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        cJSON* item_i = cJSON_GetArrayItem(questions, i);
        cJSON* item_j = cJSON_GetArrayItem(questions, j);
        cJSON* temp = cJSON_Duplicate(item_i, 1);
        cJSON_ReplaceItemInArray(questions, i, cJSON_Duplicate(item_j, 1));
        cJSON_ReplaceItemInArray(questions, j, temp);
    }
}

void handle_join_room(int client_idx, cJSON* data)
{
    if (!client_is_logged_in(client_idx)) {
        client_send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        client_send_error(client_idx, "Invalid room id");
        return;
    }

    // Get room details
    cJSON* room = storage_get_room(room_id->valuestring);
    if (!room) {
        client_send_error(client_idx, "Room not found");
        return;
    }

    // Check room status based on current time
    time_t now = time(NULL);
    cJSON* start_time_obj = cJSON_GetObjectItem(room, "start_time");
    cJSON* end_time_obj = cJSON_GetObjectItem(room, "end_time");
    cJSON* allowed_attempts_obj = cJSON_GetObjectItem(room, "allowed_attempts");
    cJSON* duration_obj = cJSON_GetObjectItem(room, "duration");
    cJSON* num_questions_obj = cJSON_GetObjectItem(room, "num_questions");
    cJSON* question_bank_id_obj = cJSON_GetObjectItem(room, "question_bank_id");

    long start_time = (long)start_time_obj->valuedouble;
    long end_time = (long)end_time_obj->valuedouble;

    if (now < start_time) {
        cJSON_Delete(room);
        client_send_error(client_idx, "Room not open yet");
        return;
    }
    if (now > end_time) {
        cJSON_Delete(room);
        client_send_error(client_idx, "Room already closed");
        return;
    }

    char username[32];
    client_get_username(client_idx, username, sizeof(username));

    // Check if user already has an active session
    ExamSession* existing_session = storage_get_exam_session(room_id->valuestring, username);
    if (existing_session) {
        // Return existing unfinished session
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "status", "SUCCESS");
        cJSON* data_obj = cJSON_CreateObject();
        cJSON_AddItemToObject(data_obj, "questions", cJSON_Duplicate(existing_session->questions, 1));
        cJSON_AddItemToObject(data_obj, "answers", cJSON_Duplicate(existing_session->answers, 1));
        cJSON_AddNumberToObject(data_obj, "start_time", existing_session->start_time);
        cJSON_AddNumberToObject(data_obj, "duration", duration_obj->valueint);
        cJSON_AddItemToObject(resp, "data", data_obj);
        client_send_response(client_idx, "RES", resp);
        cJSON_Delete(resp);
        storage_free_exam_session(existing_session);
        cJSON_Delete(room);
        return;
    }

    // Check attempt limit
    int attempts = storage_get_user_attempts(room_id->valuestring, username);
    if (attempts >= allowed_attempts_obj->valueint) {
        cJSON_Delete(room);
        client_send_error(client_idx, "Maximum attempts reached");
        return;
    }

    // Get question bank
    cJSON* all_questions = NULL;
    if (storage_get_question_bank(question_bank_id_obj->valuestring, &all_questions) != 0) {
        cJSON_Delete(room);
        client_send_error(client_idx, "Question bank not found");
        return;
    }

    int total_questions = cJSON_GetArraySize(all_questions);
    int num_questions = num_questions_obj->valueint;
    if (num_questions > total_questions) {
        num_questions = total_questions;
    }

    // Shuffle and select questions
    srand(time(NULL) + client_idx);
    shuffle_questions(all_questions, total_questions);

    cJSON* selected_questions = cJSON_CreateArray();
    cJSON* selected_questions_with_answers = cJSON_CreateArray();
    for (int i = 0; i < num_questions; i++) {
        cJSON* q = cJSON_GetArrayItem(all_questions, i);
        // For client: copy without correct_answer
        cJSON* q_copy = cJSON_Duplicate(q, 1);
        cJSON_DeleteItemFromObject(q_copy, "correct_answer");
        cJSON_AddItemToArray(selected_questions, q_copy);
        // For server session: keep correct_answer
        cJSON_AddItemToArray(selected_questions_with_answers, cJSON_Duplicate(q, 1));
    }

    // Create exam session
    ExamSession session;
    strncpy(session.room_id, room_id->valuestring, sizeof(session.room_id) - 1);
    strncpy(session.username, username, sizeof(session.username) - 1);
    session.questions = selected_questions_with_answers;
    session.answers = cJSON_CreateArray();
    for (int i = 0; i < num_questions; i++) {
        cJSON_AddItemToArray(session.answers, cJSON_CreateNumber(-1));
    }
    session.start_time = time(NULL);
    session.is_finished = 0;

    storage_save_exam_session(&session);

    // Send response with questions (without correct answers)
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "SUCCESS");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(data_obj, "questions", selected_questions);
    cJSON_AddItemToObject(data_obj, "answers", cJSON_Duplicate(session.answers, 1));
    cJSON_AddNumberToObject(data_obj, "start_time", session.start_time);
    cJSON_AddNumberToObject(data_obj, "duration", duration_obj->valueint);
    cJSON_AddItemToObject(resp, "data", data_obj);

    client_send_response(client_idx, "RES", resp);
    cJSON_Delete(resp);
    cJSON_Delete(all_questions);
    cJSON_Delete(session.questions);
    cJSON_Delete(session.answers);
    cJSON_Delete(room);
}

void handle_submit_answer(int client_idx, cJSON* data)
{
    if (!client_is_logged_in(client_idx)) {
        client_send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    cJSON* question_index = cJSON_GetObjectItem(data, "question_index");
    cJSON* answer_index = cJSON_GetObjectItem(data, "answer_index");

    if (!cJSON_IsString(room_id) || !cJSON_IsNumber(question_index) || !cJSON_IsNumber(answer_index)) {
        client_send_error(client_idx, "Invalid answer data");
        return;
    }

    char username[32];
    client_get_username(client_idx, username, sizeof(username));

    ExamSession* session = storage_get_exam_session(room_id->valuestring, username);
    if (!session) {
        client_send_error(client_idx, "No active exam session");
        return;
    }

    int q_idx = question_index->valueint;
    int ans_idx = answer_index->valueint;
    int num_questions = cJSON_GetArraySize(session->answers);

    if (q_idx < 0 || q_idx >= num_questions) {
        storage_free_exam_session(session);
        client_send_error(client_idx, "Invalid question index");
        return;
    }

    // Update answer
    cJSON_ReplaceItemInArray(session->answers, q_idx, cJSON_CreateNumber(ans_idx));
    storage_save_exam_session(session);

    client_send_success(client_idx, "Answer saved");
    storage_free_exam_session(session);
}

void handle_finish_exam(int client_idx, cJSON* data)
{
    if (!client_is_logged_in(client_idx)) {
        client_send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        client_send_error(client_idx, "Invalid room id");
        return;
    }

    char username[32];
    client_get_username(client_idx, username, sizeof(username));

    ExamSession* session = storage_get_exam_session(room_id->valuestring, username);
    if (!session) {
        client_send_error(client_idx, "No active exam session");
        return;
    }

    // Calculate score
    int num_questions = cJSON_GetArraySize(session->questions);
    int correct_count = 0;

    for (int i = 0; i < num_questions; i++) {
        cJSON* question = cJSON_GetArrayItem(session->questions, i);
        cJSON* user_answer = cJSON_GetArrayItem(session->answers, i);
        cJSON* correct_answer = cJSON_GetObjectItem(question, "correct_answer");

        if (user_answer && correct_answer && cJSON_IsNumber(user_answer) && cJSON_IsNumber(correct_answer)) {
            if ((int)user_answer->valuedouble == (int)correct_answer->valuedouble) {
                correct_count++;
            }
        }
    }

    int score = (num_questions > 0) ? (correct_count * 100) / num_questions : 0;

    // Save result
    RoomResult result;
    strncpy(result.room_id, room_id->valuestring, sizeof(result.room_id) - 1);
    strncpy(result.username, username, sizeof(result.username) - 1);
    result.score = score;
    result.num_questions = num_questions;
    result.correct_count = correct_count;
    result.timestamp = time(NULL);
    storage_save_result(&result);

    // Delete session after finishing
    storage_delete_exam_session(room_id->valuestring, username);

    // Send result
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "SUCCESS");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "score", score);
    cJSON_AddNumberToObject(data_obj, "correct_count", correct_count);
    cJSON_AddNumberToObject(data_obj, "total_questions", num_questions);
    cJSON_AddItemToObject(resp, "data", data_obj);

    client_send_response(client_idx, "RES", resp);
    cJSON_Delete(resp);
    storage_free_exam_session(session);
}

void handle_get_exam_state(int client_idx, cJSON* data)
{
    if (!client_is_logged_in(client_idx)) {
        client_send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        client_send_error(client_idx, "Invalid room id");
        return;
    }

    char username[32];
    client_get_username(client_idx, username, sizeof(username));

    ExamSession* session = storage_get_exam_session(room_id->valuestring, username);
    if (!session) {
        client_send_error(client_idx, "No exam session found");
        return;
    }

    cJSON* room = storage_get_room(room_id->valuestring);
    if (!room) {
        storage_free_exam_session(session);
        client_send_error(client_idx, "Room not found");
        return;
    }

    cJSON* duration_obj = cJSON_GetObjectItem(room, "duration");

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "SUCCESS");
    cJSON* data_obj = cJSON_CreateObject();

    // Remove correct answers from questions
    cJSON* questions_no_answers = cJSON_Duplicate(session->questions, 1);
    int num_questions = cJSON_GetArraySize(questions_no_answers);
    for (int i = 0; i < num_questions; i++) {
        cJSON* q = cJSON_GetArrayItem(questions_no_answers, i);
        cJSON_DeleteItemFromObject(q, "correct_answer");
    }

    cJSON_AddItemToObject(data_obj, "questions", questions_no_answers);
    cJSON_AddItemToObject(data_obj, "answers", cJSON_Duplicate(session->answers, 1));
    cJSON_AddNumberToObject(data_obj, "start_time", session->start_time);
    cJSON_AddNumberToObject(data_obj, "duration", duration_obj->valueint);
    cJSON_AddBoolToObject(data_obj, "is_finished", session->is_finished);
    cJSON_AddItemToObject(resp, "data", data_obj);

    client_send_response(client_idx, "RES", resp);
    cJSON_Delete(resp);
    cJSON_Delete(room);
    storage_free_exam_session(session);
}
