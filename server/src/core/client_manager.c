#include "client_manager.h"
#include "auth_handler.h"
#include "exam_handler.h"
#include "net.h"
#include "protocol.h"
#include "question_handler.h"
#include "room_handler.h"
#include "storage.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static ClientState clients[MAX_CLIENTS];
static struct pollfd fds[MAX_CLIENTS + 1];
static int client_count = 0;

// Forward declarations
static void process_message(int client_idx, const char* msg_type, cJSON* payload);

void client_manager_init()
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].is_logged_in = 0;
        fds[i + 1].fd = -1;
    }
}

ClientState* client_get_state(int client_idx)
{
    if (client_idx >= 0 && client_idx < MAX_CLIENTS) {
        return &clients[client_idx];
    }
    return NULL;
}

int client_get_count()
{
    return client_count;
}

struct pollfd* client_get_pollfds()
{
    return fds;
}

void client_add(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (new_socket < 0)
        return;

    printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

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

void client_remove(int client_idx)
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

void client_handle_activity(int client_idx)
{
    char msg_type[4];
    cJSON* payload = NULL;
    int res = receive_packet(clients[client_idx].fd, msg_type, &payload);

    if (res == 0) {
        printf("Received %s from client %d\n", msg_type, client_idx);
        fflush(stdout);
        process_message(client_idx, msg_type, payload);
        cJSON_Delete(payload);
    } else if (res == -3) {
        printf("Parse error from client %d\n", client_idx);
    } else {
        printf("Client %d disconnected\n", client_idx);
        fflush(stdout);
        handle_logout(client_idx);
        client_remove(client_idx);
    }
}

int client_is_logged_in(int client_idx)
{
    return clients[client_idx].is_logged_in;
}

int client_check_admin(int client_idx)
{
    if (!clients[client_idx].is_logged_in)
        return 0;
    return strcmp(storage_get_role(clients[client_idx].username), "admin") == 0;
}

void client_get_username(int client_idx, char* buffer, int buffer_size)
{
    if (buffer && buffer_size > 0) {
        strncpy(buffer, clients[client_idx].username, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
}

void client_set_logged_in(int client_idx, const char* username)
{
    clients[client_idx].is_logged_in = 1;
    strncpy(clients[client_idx].username, username, sizeof(clients[client_idx].username) - 1);
    clients[client_idx].username[sizeof(clients[client_idx].username) - 1] = '\0';
}

void client_logout(int client_idx)
{
    if (clients[client_idx].is_logged_in) {
        printf("User %s logged out\n", clients[client_idx].username);
        clients[client_idx].is_logged_in = 0;
        clients[client_idx].username[0] = '\0';
    }
}

void client_kick_duplicate_user(const char* username, int except_idx)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != except_idx && clients[i].is_logged_in && strcmp(clients[i].username, username) == 0) {
            client_send_error(i, "Logged in from another location");
            printf("Kicking user %s (client %d)\n", username, i);
            client_remove(i);
        }
    }
}

void client_send_response(int client_idx, const char* msg_type, cJSON* payload)
{
    send_packet(clients[client_idx].fd, msg_type, payload);
}

void client_send_error(int client_idx, const char* msg)
{
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ERROR");
    cJSON_AddStringToObject(resp, "message", msg);
    send_packet(clients[client_idx].fd, MSG_TYPE_ERR, resp);
    cJSON_Delete(resp);
}

void client_send_success(int client_idx, const char* msg)
{
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "SUCCESS");
    cJSON_AddStringToObject(resp, "message", msg);
    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
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
                client_send_success(client_idx, "Logged out");
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
