#include "room_handler.h"
#include "client_manager.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void handle_create_room(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* name = cJSON_GetObjectItem(data, "room_name");
    cJSON* start = cJSON_GetObjectItem(data, "start_time");
    cJSON* end = cJSON_GetObjectItem(data, "end_time");
    cJSON* bank = cJSON_GetObjectItem(data, "question_bank_id");
    cJSON* num_q = cJSON_GetObjectItem(data, "num_questions");
    cJSON* attempts = cJSON_GetObjectItem(data, "allowed_attempts");
    cJSON* duration = cJSON_GetObjectItem(data, "duration");
    cJSON* show_answers = cJSON_GetObjectItem(data, "show_answers");

    if (!cJSON_IsString(name) || !cJSON_IsNumber(start) || !cJSON_IsNumber(end) || !cJSON_IsString(bank)) {
        client_send_error(client_idx, "Invalid room data");
        return;
    }

    Room room;
    memset(&room, 0, sizeof(room));
    snprintf(room.id, sizeof(room.id), "room_%ld", time(NULL));
    strncpy(room.name, name->valuestring, sizeof(room.name) - 1);
    room.start_time = (long)start->valuedouble;
    room.end_time = (long)end->valuedouble;
    strncpy(room.question_bank_id, bank->valuestring, sizeof(room.question_bank_id) - 1);
    strcpy(room.status, "OPEN");
    room.num_questions = num_q ? num_q->valueint : 10;
    room.allowed_attempts = attempts ? attempts->valueint : 1;
    room.duration = duration ? duration->valueint : 0;
    room.show_answers = show_answers ? show_answers->valueint : 0;

    if (storage_save_room(&room) == 0) {
        client_send_success(client_idx, "Room created");
    } else {
        client_send_error(client_idx, "Failed to create room");
    }
}

void handle_list_rooms(int client_idx)
{
    cJSON* rooms = cJSON_CreateArray();
    storage_get_rooms(rooms);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "SUCCESS");
    cJSON_AddItemToObject(resp, "data", rooms);

    client_send_response(client_idx, "RES", resp);
    cJSON_Delete(resp);
}

void handle_get_room_stats(int client_idx, cJSON* data)
{
    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        client_send_error(client_idx, "Invalid room id");
        return;
    }

    cJSON* results = cJSON_CreateArray();
    storage_get_room_results(room_id->valuestring, results);

    // Calculate stats
    int total_attempts = cJSON_GetArraySize(results);
    double total_score = 0;
    cJSON* item;
    cJSON_ArrayForEach(item, results)
    {
        cJSON* score = cJSON_GetObjectItem(item, "score");
        if (score)
            total_score += score->valueint;
    }
    double avg_score = (total_attempts > 0) ? (total_score / total_attempts) : 0;

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "SUCCESS");

    cJSON* data_obj = cJSON_CreateObject();

    // Add Room Info
    cJSON* room_info = storage_get_room(room_id->valuestring);
    if (room_info) {
        cJSON_AddItemToObject(data_obj, "room", room_info);
    }

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "total_attempts", total_attempts);
    cJSON_AddNumberToObject(stats, "average_score", avg_score);
    cJSON_AddItemToObject(data_obj, "stats", stats);

    cJSON_AddItemToObject(data_obj, "results", cJSON_Duplicate(results, 1));

    // Check if user has active exam
    char username[32];
    client_get_username(client_idx, username, sizeof(username));
    ExamSession* session = storage_get_exam_session(room_id->valuestring, username);
    int has_active_exam = (session != NULL) ? 1 : 0;
    cJSON_AddBoolToObject(data_obj, "has_active_exam", has_active_exam);
    if (session) {
        storage_free_exam_session(session);
    }

    cJSON_AddItemToObject(resp, "data", data_obj);
    client_send_response(client_idx, "RES", resp);
    cJSON_Delete(resp);
    cJSON_Delete(results);
}

void handle_delete_room(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        client_send_error(client_idx, "Invalid room id");
        return;
    }

    if (storage_delete_room(room_id->valuestring) == 0) {
        client_send_success(client_idx, "Room deleted");
    } else {
        client_send_error(client_idx, "Failed to delete room");
    }
}

void handle_close_room(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        client_send_error(client_idx, "Invalid room id");
        return;
    }

    if (storage_update_room_status(room_id->valuestring, "CLOSED") == 0) {
        client_send_success(client_idx, "Room closed");
    } else {
        client_send_error(client_idx, "Failed to close room");
    }
}
