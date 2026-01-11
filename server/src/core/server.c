#include "server.h"
#include "net.h"
#include "protocol.h"
#include "storage.h"
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 100
#define POLL_TIMEOUT_MS -1
#define DEFAULT_PORT 8080

typedef struct
{
    int fd;
    char username[32];
    int is_logged_in;
} ClientState;

static ClientState clients[MAX_CLIENTS];
static struct pollfd fds[MAX_CLIENTS + 1]; // +1 for server socket
static int client_count = 0;

// Forward declarations of helper functions
static void init_clients();
static void add_new_client(int server_fd);
static void handle_client_activity(int client_idx);
static void process_message(int client_idx, const char* msg_type, cJSON* payload);

void handle_login(int client_idx, cJSON* data);
void handle_register(int client_idx, cJSON* data);
void handle_logout(int client_idx);
void handle_create_room(int client_idx, cJSON* data);
void handle_list_rooms(int client_idx);
void handle_import_questions(int client_idx, cJSON* data);
void handle_list_question_banks(int client_idx);
void handle_get_question_bank(int client_idx, cJSON* data);
void handle_update_question_bank(int client_idx, cJSON* data);
void handle_delete_question_bank(int client_idx, cJSON* data);
void handle_get_room_stats(int client_idx, cJSON* data);
void handle_close_room(int client_idx, cJSON* data);
void handle_delete_room(int client_idx, cJSON* data);
void handle_join_room(int client_idx, cJSON* data);
void handle_submit_answer(int client_idx, cJSON* data);
void handle_finish_exam(int client_idx, cJSON* data);
void handle_get_exam_state(int client_idx, cJSON* data);
void remove_client(int client_idx);
void send_error(int client_idx, const char* msg);
void send_success(int client_idx, const char* msg);

int main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    storage_init();
    server_start(port);
    return 0;
}

void server_start(int port)
{
    int server_fd = net_listen(port);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to start server\n");
        exit(1);
    }

    init_clients();

    // Setup poll server fd
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Server loop started on port %d...\n", port);
    fflush(stdout);

    while (1) {
        int ret = poll(fds, MAX_CLIENTS + 1, POLL_TIMEOUT_MS);
        if (ret < 0) {
            perror("poll");
            break;
        }

        // Check for new connection
        if (fds[0].revents & POLLIN) {
            add_new_client(server_fd);
        }

        // Check clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1 && (fds[i + 1].revents & POLLIN)) {
                handle_client_activity(i);
            }
        }
    }

    close(server_fd);
}

static void init_clients()
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].is_logged_in = 0;
        fds[i + 1].fd = -1;
    }
}

static void add_new_client(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (new_socket < 0)
        return;

    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // Find slot
    int added = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = new_socket;
            clients[i].is_logged_in = 0;
            clients[i].username[0] = '\0';

            fds[i + 1].fd = new_socket;
            fds[i + 1].events = POLLIN;

            client_count++;
            added = 1;
            break;
        }
    }

    if (!added) {
        printf("Max clients reached. Rejecting connection.\n");
        close(new_socket);
    }
}

static void handle_client_activity(int i)
{
    char msg_type[4];
    cJSON* payload = NULL;
    int res = receive_packet(clients[i].fd, msg_type, &payload);

    if (res == 0) {
        printf("Received %s from client %d\n", msg_type, i);
        fflush(stdout);
        process_message(i, msg_type, payload);
        cJSON_Delete(payload);
    } else if (res == -3) {
        printf("Parse error from client %d\n", i);
    } else {
        printf("Client %d disconnected\n", i);
        fflush(stdout);
        handle_logout(i);
        remove_client(i);
    }
}

static void process_message(int client_idx, const char* msg_type, cJSON* payload)
{
    if (strcmp(msg_type, MSG_TYPE_REQ) == 0 || strcmp(msg_type, MSG_TYPE_UPD) == 0) {
        cJSON* action_item = cJSON_GetObjectItem(payload, JSON_KEY_ACTION);
        if (cJSON_IsString(action_item)) {
            char* action = action_item->valuestring;
            cJSON* data = cJSON_GetObjectItem(payload, JSON_KEY_DATA);

            if (strcmp(action, ACTION_LOGIN) == 0) {
                handle_login(client_idx, data);
            } else if (strcmp(action, ACTION_REGISTER) == 0) {
                handle_register(client_idx, data);
            } else if (strcmp(action, ACTION_LOGOUT) == 0) {
                handle_logout(client_idx);
                send_success(client_idx, "Logged out");
            } else if (strcmp(action, ACTION_CREATE_ROOM) == 0) {
                handle_create_room(client_idx, data);
            } else if (strcmp(action, ACTION_LIST_ROOMS) == 0) {
                handle_list_rooms(client_idx);
            } else if (strcmp(action, ACTION_IMPORT_QUESTIONS) == 0) {
                handle_import_questions(client_idx, data);
            } else if (strcmp(action, ACTION_LIST_QUESTION_BANKS) == 0) {
                handle_list_question_banks(client_idx);
            } else if (strcmp(action, ACTION_GET_QUESTION_BANK) == 0) {
                handle_get_question_bank(client_idx, data);
            } else if (strcmp(action, ACTION_UPDATE_QUESTION_BANK) == 0) {
                handle_update_question_bank(client_idx, data);
            } else if (strcmp(action, ACTION_DELETE_QUESTION_BANK) == 0) {
                handle_delete_question_bank(client_idx, data);
            } else if (strcmp(action, ACTION_GET_ROOM_STATS) == 0) {
                handle_get_room_stats(client_idx, data);
            } else if (strcmp(action, ACTION_DELETE_ROOM) == 0) {
                handle_delete_room(client_idx, data);
            } else if (strcmp(action, ACTION_JOIN_ROOM) == 0) {
                handle_join_room(client_idx, data);
            } else if (strcmp(action, ACTION_SUBMIT_ANSWER) == 0) {
                handle_submit_answer(client_idx, data);
            } else if (strcmp(action, ACTION_FINISH_EXAM) == 0) {
                handle_finish_exam(client_idx, data);
            } else if (strcmp(action, ACTION_GET_EXAM_STATE) == 0) {
                handle_get_exam_state(client_idx, data);
            }
        }
    } else if (strcmp(msg_type, MSG_TYPE_HBT) == 0) {
        send_packet(clients[client_idx].fd, MSG_TYPE_HBT, payload);
    }
}

void remove_client(int client_idx)
{
    if (clients[client_idx].fd != -1) {
        close(clients[client_idx].fd);
        clients[client_idx].fd = -1;
    }
    clients[client_idx].is_logged_in = 0;
    clients[client_idx].username[0] = '\0';
    fds[client_idx + 1].fd = -1;
    client_count--;
}

void send_error(int client_idx, const char* msg)
{
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "ERROR");
    cJSON_AddStringToObject(resp, JSON_KEY_MESSAGE, msg);
    send_packet(clients[client_idx].fd, MSG_TYPE_ERR, resp);
    cJSON_Delete(resp);
}

void send_success(int client_idx, const char* msg)
{
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
    cJSON_AddStringToObject(resp, JSON_KEY_MESSAGE, msg);
    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
}

void handle_login(int client_idx, cJSON* data)
{
    cJSON* user_item = cJSON_GetObjectItem(data, JSON_KEY_USERNAME);
    cJSON* pass_item = cJSON_GetObjectItem(data, JSON_KEY_PASSWORD);

    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item)) {
        send_error(client_idx, "Invalid format");
        return;
    }

    char* username = user_item->valuestring;
    char* password = pass_item->valuestring;

    // Check concurrent login
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_logged_in && strcmp(clients[i].username, username) == 0) {
            send_error(i, "Logged in from another location");
            printf("Kicking user %s (client %d)\n", username, i);
            remove_client(i);
        }
    }

    if (storage_check_credentials(username, password)) {
        clients[client_idx].is_logged_in = 1;
        strncpy(clients[client_idx].username, username, sizeof(clients[client_idx].username) - 1);
        clients[client_idx].username[sizeof(clients[client_idx].username) - 1] = '\0';

        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
        cJSON_AddStringToObject(resp, JSON_KEY_MESSAGE, "Login successful");

        cJSON* data_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(data_obj, "role", storage_get_role(username));
        cJSON_AddItemToObject(resp, JSON_KEY_DATA, data_obj);

        send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
        cJSON_Delete(resp);

        printf("User %s logged in as %s\n", username, storage_get_role(username));
    } else {
        send_error(client_idx, "Invalid credentials");
    }
}

void handle_register(int client_idx, cJSON* data)
{
    cJSON* user_item = cJSON_GetObjectItem(data, JSON_KEY_USERNAME);
    cJSON* pass_item = cJSON_GetObjectItem(data, JSON_KEY_PASSWORD);

    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item)) {
        send_error(client_idx, "Invalid format");
        return;
    }

    char* username = user_item->valuestring;
    char* password = pass_item->valuestring;

    int res = storage_add_user(username, password, NULL);
    if (res == 0) {
        send_success(client_idx, "Register successful");
        printf("User %s registered\n", username);
    } else if (res == -2) {
        send_error(client_idx, "Username already exists");
    } else {
        send_error(client_idx, "Registration failed");
    }
}

void handle_logout(int client_idx)
{
    if (clients[client_idx].is_logged_in) {
        printf("User %s logged out\n", clients[client_idx].username);
        clients[client_idx].is_logged_in = 0;
        clients[client_idx].username[0] = '\0';
    }
}

void handle_create_room(int client_idx, cJSON* data)
{
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* name = cJSON_GetObjectItem(data, "room_name");
    cJSON* start = cJSON_GetObjectItem(data, "start_time");
    cJSON* end = cJSON_GetObjectItem(data, "end_time");
    cJSON* bank = cJSON_GetObjectItem(data, "question_bank_id");
    cJSON* num_q = cJSON_GetObjectItem(data, "num_questions");
    cJSON* attempts = cJSON_GetObjectItem(data, "allowed_attempts");
    cJSON* duration = cJSON_GetObjectItem(data, "duration");

    if (!cJSON_IsString(name) || !cJSON_IsNumber(start) || !cJSON_IsNumber(end) || !cJSON_IsString(bank)) {
        send_error(client_idx, "Invalid room data");
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

    if (storage_save_room(&room) == 0) {
        send_success(client_idx, "Room created");
    } else {
        send_error(client_idx, "Failed to create room");
    }
}

void handle_list_rooms(int client_idx)
{
    // Both admin and participant can list rooms, but maybe filter?
    // Spec says Admin "Request: LIST_ROOMS ... Response: LIST of rooms"
    // Also Participant needs to see rooms. So allow all logged in.

    cJSON* rooms = cJSON_CreateArray();
    storage_get_rooms(rooms);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
    cJSON_AddItemToObject(resp, JSON_KEY_DATA, rooms);

    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
}

void handle_import_questions(int client_idx, cJSON* data)
{
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_name = cJSON_GetObjectItem(data, "bank_name");
    cJSON* questions = cJSON_GetObjectItem(data, "questions");

    if (!cJSON_IsString(bank_name) || !cJSON_IsArray(questions)) {
        send_error(client_idx, "Invalid question data");
        return;
    }

    if (storage_save_question_bank(bank_name->valuestring, questions) == 0) {
        send_success(client_idx, "Questions imported");
    } else {
        send_error(client_idx, "Failed to import questions");
    }
}

void handle_list_question_banks(int client_idx)
{
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* banks = cJSON_CreateArray();
    if (storage_list_question_banks(banks) == 0) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
        cJSON_AddItemToObject(resp, JSON_KEY_DATA, banks);
        send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
        cJSON_Delete(resp);
    } else {
        cJSON_Delete(banks);
        send_error(client_idx, "Failed to list question banks");
    }
}

void handle_get_question_bank(int client_idx, cJSON* data)
{
    // Both admin and participant may need questions? Spec says rooms load
    // questions from bank. For now assuming Admin editor needs this.
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_id = cJSON_GetObjectItem(data, "bank_id");
    if (!cJSON_IsString(bank_id)) {
        send_error(client_idx, "Invalid bank id");
        return;
    }

    cJSON* questions = NULL;
    if (storage_get_question_bank(bank_id->valuestring, &questions) == 0) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
        cJSON_AddItemToObject(resp, JSON_KEY_DATA, questions);
        send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
        cJSON_Delete(resp);
    } else {
        send_error(client_idx, "Bank not found");
    }
}

void handle_update_question_bank(int client_idx, cJSON* data)
{
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_id = cJSON_GetObjectItem(data, "bank_id");
    cJSON* questions = cJSON_GetObjectItem(data, "questions");

    if (!cJSON_IsString(bank_id) || !cJSON_IsArray(questions)) {
        send_error(client_idx, "Invalid data");
        return;
    }

    if (storage_update_question_bank(bank_id->valuestring, questions) == 0) {
        send_success(client_idx, "Question bank updated");
    } else {
        send_error(client_idx, "Failed to update bank");
    }
}

void handle_delete_question_bank(int client_idx, cJSON* data)
{
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_id = cJSON_GetObjectItem(data, "bank_id");
    if (!cJSON_IsString(bank_id)) {
        send_error(client_idx, "Invalid bank id");
        return;
    }

    if (storage_delete_question_bank(bank_id->valuestring) == 0) {
        send_success(client_idx, "Question bank deleted");
    } else {
        send_error(client_idx, "Failed to delete bank");
    }
}

void handle_get_room_stats(int client_idx, cJSON* data)
{
    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        send_error(client_idx, "Invalid room id");
        return;
    }

    cJSON* results = cJSON_CreateArray();
    storage_get_room_results(room_id->valuestring, results);

    // Calculate basic stats
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
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");

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

    // Add full results
    cJSON_AddItemToObject(data_obj, "results", cJSON_Duplicate(results, 1));

    // Check if user has an active (unfinished) exam session
    ExamSession* session = storage_get_exam_session(room_id->valuestring, clients[client_idx].username);
    int has_active_exam = (session != NULL) ? 1 : 0;
    cJSON_AddBoolToObject(data_obj, "has_active_exam", has_active_exam);
    if (session) {
        storage_free_exam_session(session);
    }

    cJSON_AddItemToObject(resp, JSON_KEY_DATA, data_obj);
    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
    cJSON_Delete(results);
}

void handle_delete_room(int client_idx, cJSON* data)
{
    if (strcmp(storage_get_role(clients[client_idx].username), "admin") != 0) {
        send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        send_error(client_idx, "Invalid room id");
        return;
    }

    if (storage_delete_room(room_id->valuestring) == 0) {
        send_success(client_idx, "Room deleted");
    } else {
        send_error(client_idx, "Failed to delete room");
    }
}

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
    if (!clients[client_idx].is_logged_in) {
        send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        send_error(client_idx, "Invalid room id");
        return;
    }

    // Get room details
    cJSON* room = storage_get_room(room_id->valuestring);
    if (!room) {
        send_error(client_idx, "Room not found");
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
        send_error(client_idx, "Room not open yet");
        return;
    }
    if (now > end_time) {
        cJSON_Delete(room);
        send_error(client_idx, "Room already closed");
        return;
    }

    // Check if user already has an active session
    ExamSession* existing_session = storage_get_exam_session(room_id->valuestring, clients[client_idx].username);
    if (existing_session) {
        // Return existing unfinished session
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
        cJSON* data_obj = cJSON_CreateObject();
        cJSON_AddItemToObject(data_obj, "questions", cJSON_Duplicate(existing_session->questions, 1));
        cJSON_AddItemToObject(data_obj, "answers", cJSON_Duplicate(existing_session->answers, 1));
        cJSON_AddNumberToObject(data_obj, "start_time", existing_session->start_time);
        cJSON_AddNumberToObject(data_obj, "duration", duration_obj->valueint);
        cJSON_AddItemToObject(resp, JSON_KEY_DATA, data_obj);
        send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
        cJSON_Delete(resp);
        storage_free_exam_session(existing_session);
        cJSON_Delete(room);
        return;
    }

    // Check attempt limit
    int attempts = storage_get_user_attempts(room_id->valuestring, clients[client_idx].username);
    if (attempts >= allowed_attempts_obj->valueint) {
        cJSON_Delete(room);
        send_error(client_idx, "Maximum attempts reached");
        return;
    }

    // Get question bank
    cJSON* all_questions = NULL;
    if (storage_get_question_bank(question_bank_id_obj->valuestring, &all_questions) != 0) {
        cJSON_Delete(room);
        send_error(client_idx, "Question bank not found");
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
    strncpy(session.username, clients[client_idx].username, sizeof(session.username) - 1);
    session.questions = selected_questions_with_answers; // Save with correct answers for grading
    session.answers = cJSON_CreateArray();
    for (int i = 0; i < num_questions; i++) {
        cJSON_AddItemToArray(session.answers, cJSON_CreateNumber(-1)); // -1 means unanswered
    }
    session.start_time = time(NULL);
    session.is_finished = 0;

    storage_save_exam_session(&session);

    // Send response with questions (without correct answers)
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(data_obj, "questions", selected_questions);
    cJSON_AddItemToObject(data_obj, "answers", cJSON_Duplicate(session.answers, 1));
    cJSON_AddNumberToObject(data_obj, "start_time", session.start_time);
    cJSON_AddNumberToObject(data_obj, "duration", duration_obj->valueint);
    cJSON_AddItemToObject(resp, JSON_KEY_DATA, data_obj);

    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
    cJSON_Delete(all_questions);
    cJSON_Delete(session.questions);
    cJSON_Delete(session.answers);
    cJSON_Delete(room);
}

void handle_submit_answer(int client_idx, cJSON* data)
{
    if (!clients[client_idx].is_logged_in) {
        send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    cJSON* question_index = cJSON_GetObjectItem(data, "question_index");
    cJSON* answer_index = cJSON_GetObjectItem(data, "answer_index");

    if (!cJSON_IsString(room_id) || !cJSON_IsNumber(question_index) || !cJSON_IsNumber(answer_index)) {
        send_error(client_idx, "Invalid answer data");
        return;
    }

    ExamSession* session = storage_get_exam_session(room_id->valuestring, clients[client_idx].username);
    if (!session) {
        send_error(client_idx, "No active exam session");
        return;
    }

    int q_idx = question_index->valueint;
    int ans_idx = answer_index->valueint;
    int num_questions = cJSON_GetArraySize(session->answers);

    if (q_idx < 0 || q_idx >= num_questions) {
        storage_free_exam_session(session);
        send_error(client_idx, "Invalid question index");
        return;
    }

    // Update answer
    cJSON_ReplaceItemInArray(session->answers, q_idx, cJSON_CreateNumber(ans_idx));
    storage_save_exam_session(session);

    send_success(client_idx, "Answer saved");
    storage_free_exam_session(session);
}

void handle_finish_exam(int client_idx, cJSON* data)
{
    if (!clients[client_idx].is_logged_in) {
        send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        send_error(client_idx, "Invalid room id");
        return;
    }

    ExamSession* session = storage_get_exam_session(room_id->valuestring, clients[client_idx].username);
    if (!session) {
        send_error(client_idx, "No active exam session");
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
    strncpy(result.username, clients[client_idx].username, sizeof(result.username) - 1);
    result.score = score;
    result.num_questions = num_questions;
    result.correct_count = correct_count;
    result.timestamp = time(NULL);
    storage_save_result(&result);

    // Delete session after finishing
    storage_delete_exam_session(room_id->valuestring, clients[client_idx].username);

    // Send result
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(data_obj, "score", score);
    cJSON_AddNumberToObject(data_obj, "correct_count", correct_count);
    cJSON_AddNumberToObject(data_obj, "total_questions", num_questions);
    cJSON_AddItemToObject(resp, JSON_KEY_DATA, data_obj);

    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
    storage_free_exam_session(session);
}

void handle_get_exam_state(int client_idx, cJSON* data)
{
    if (!clients[client_idx].is_logged_in) {
        send_error(client_idx, "Not logged in");
        return;
    }

    cJSON* room_id = cJSON_GetObjectItem(data, "room_id");
    if (!cJSON_IsString(room_id)) {
        send_error(client_idx, "Invalid room id");
        return;
    }

    ExamSession* session = storage_get_exam_session(room_id->valuestring, clients[client_idx].username);
    if (!session) {
        send_error(client_idx, "No exam session found");
        return;
    }

    cJSON* room = storage_get_room(room_id->valuestring);
    if (!room) {
        storage_free_exam_session(session);
        send_error(client_idx, "Room not found");
        return;
    }

    cJSON* duration_obj = cJSON_GetObjectItem(room, "duration");

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
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
    cJSON_AddItemToObject(resp, JSON_KEY_DATA, data_obj);

    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
    cJSON_Delete(room);
    storage_free_exam_session(session);
}

