#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include "cJSON.h"
#include <poll.h>

#define MAX_CLIENTS 100

typedef struct {
    int fd;
    char username[32];
    int is_logged_in;
} ClientState;

// Client state access
void client_manager_init();
ClientState* client_get_state(int client_idx);
int client_get_count();
struct pollfd* client_get_pollfds();

// Client operations
void client_add(int server_fd);
void client_remove(int client_idx);
void client_handle_activity(int client_idx);

// Client info getters
int client_is_logged_in(int client_idx);
int client_check_admin(int client_idx);
void client_get_username(int client_idx, char* buffer, int buffer_size);

// Client actions
void client_set_logged_in(int client_idx, const char* username);
void client_logout(int client_idx);
void client_kick_duplicate_user(const char* username, int except_idx);

// Messaging
void client_send_response(int client_idx, const char* msg_type, cJSON* payload);
void client_send_error(int client_idx, const char* msg);
void client_send_success(int client_idx, const char* msg);

#endif
