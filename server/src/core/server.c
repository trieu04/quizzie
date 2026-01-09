#include "server.h"
#include "net.h"
#include "storage.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define POLL_TIMEOUT_MS -1
#define DEFAULT_PORT 8080

typedef struct {
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
static void process_message(int client_idx, const char *msg_type, cJSON *payload);

void handle_login(int client_idx, cJSON *data);
void handle_register(int client_idx, cJSON *data);
void handle_logout(int client_idx);
void remove_client(int client_idx);
void send_error(int client_idx, const char *msg);
void send_success(int client_idx, const char *msg);

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    storage_init();
    server_start(port);
    return 0;
}

void server_start(int port) {
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

static void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].is_logged_in = 0;
        fds[i + 1].fd = -1;
    }
}

static void add_new_client(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    
    if (new_socket < 0) return;

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

static void handle_client_activity(int i) {
    char msg_type[4];
    cJSON *payload = NULL;
    int res = receive_packet(clients[i].fd, msg_type, &payload);
    
    if (res == 0) {
        printf("Received %s from client %d\n", msg_type, i); fflush(stdout);
        process_message(i, msg_type, payload);
        cJSON_Delete(payload);
    } else if (res == -3) {
         printf("Parse error from client %d\n", i);
    } else {
        printf("Client %d disconnected\n", i); fflush(stdout);
        handle_logout(i);
        remove_client(i);
    }
}

static void process_message(int client_idx, const char *msg_type, cJSON *payload) {
    if (strcmp(msg_type, MSG_TYPE_REQ) == 0 || strcmp(msg_type, MSG_TYPE_UPD) == 0) {
        cJSON *action_item = cJSON_GetObjectItem(payload, JSON_KEY_ACTION);
        if (cJSON_IsString(action_item)) {
            char *action = action_item->valuestring;
            cJSON *data = cJSON_GetObjectItem(payload, JSON_KEY_DATA);
            
            if (strcmp(action, ACTION_LOGIN) == 0) {
                handle_login(client_idx, data);
            } else if (strcmp(action, ACTION_REGISTER) == 0) {
                handle_register(client_idx, data);
            } else if (strcmp(action, ACTION_LOGOUT) == 0) {
                handle_logout(client_idx);
                send_success(client_idx, "Logged out");
            }
        }
    } else if (strcmp(msg_type, MSG_TYPE_HBT) == 0) {
        send_packet(clients[client_idx].fd, MSG_TYPE_HBT, payload);
    }
}

void remove_client(int client_idx) {
    if (clients[client_idx].fd != -1) {
        close(clients[client_idx].fd);
        clients[client_idx].fd = -1;
    }
    clients[client_idx].is_logged_in = 0;
    clients[client_idx].username[0] = '\0';
    fds[client_idx + 1].fd = -1;
    client_count--;
}

void send_error(int client_idx, const char *msg) {
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "ERROR");
    cJSON_AddStringToObject(resp, JSON_KEY_MESSAGE, msg);
    send_packet(clients[client_idx].fd, MSG_TYPE_ERR, resp);
    cJSON_Delete(resp);
}

void send_success(int client_idx, const char *msg) {
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, JSON_KEY_STATUS, "SUCCESS");
    cJSON_AddStringToObject(resp, JSON_KEY_MESSAGE, msg);
    send_packet(clients[client_idx].fd, MSG_TYPE_RES, resp);
    cJSON_Delete(resp);
}

void handle_login(int client_idx, cJSON *data) {
    cJSON *user_item = cJSON_GetObjectItem(data, JSON_KEY_USERNAME);
    cJSON *pass_item = cJSON_GetObjectItem(data, JSON_KEY_PASSWORD);
    
    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item)) {
        send_error(client_idx, "Invalid format");
        return;
    }
    
    char *username = user_item->valuestring;
    char *password = pass_item->valuestring;
    
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
        send_success(client_idx, "Login successful");
        printf("User %s logged in\n", username);
    } else {
        send_error(client_idx, "Invalid credentials");
    }
}

void handle_register(int client_idx, cJSON *data) {
    cJSON *user_item = cJSON_GetObjectItem(data, JSON_KEY_USERNAME);
    cJSON *pass_item = cJSON_GetObjectItem(data, JSON_KEY_PASSWORD);
    
    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item)) {
        send_error(client_idx, "Invalid format");
        return;
    }
    
    char *username = user_item->valuestring;
    char *password = pass_item->valuestring;
    
    int res = storage_add_user(username, password);
    if (res == 0) {
        send_success(client_idx, "Register successful");
        printf("User %s registered\n", username);
    } else if (res == -2) {
        send_error(client_idx, "Username already exists");
    } else {
        send_error(client_idx, "Registration failed");
    }
}

void handle_logout(int client_idx) {
    if (clients[client_idx].is_logged_in) {
        printf("User %s logged out\n", clients[client_idx].username);
        clients[client_idx].is_logged_in = 0;
        clients[client_idx].username[0] = '\0';
    }
}
