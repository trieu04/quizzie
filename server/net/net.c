#include "../include/net.h"
#include "../include/server.h"
#include "../include/storage.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

int net_setup(ServerContext* ctx) {
    ctx->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_fd < 0) {
        LOG_ERROR("socket() failed");
        perror("socket");
        return -1;
    }

    // Enable address reuse
    int opt = 1;
    setsockopt(ctx->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(ctx->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("bind() failed");
        perror("bind");
        return -1;
    }
    if (listen(ctx->server_fd, SERVER_BACKLOG) < 0) {
        LOG_ERROR("listen() failed");
        perror("listen");
        return -1;
    }

    // Set non-blocking
    int flags = fcntl(ctx->server_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(ctx->server_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR("fcntl() failed setting non-blocking");
        perror("fcntl");
    }

    // Setup epoll
    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd < 0) {
        LOG_ERROR("epoll_create1() failed");
        perror("epoll_create1");
        return -1;
    }
    struct epoll_event event = {0};
    event.events = EPOLLIN;
    event.data.fd = ctx->server_fd;
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->server_fd, &event) < 0) {
        LOG_ERROR("epoll_ctl() failed");
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}

int net_accept_client(ServerContext* ctx) {
    if (ctx->client_count >= MAX_CLIENTS) {
        LOG_ERROR("Max clients reached, rejecting connection");
        int temp_sock = accept(ctx->server_fd, NULL, NULL);
        if (temp_sock >= 0) {
            close(temp_sock);
        }
        return -1;
    }

    int client_sock = accept(ctx->server_fd, NULL, NULL);
    if (client_sock >= 0) {
        int flags = fcntl(client_sock, F_GETFL, 0);
        if (flags < 0 || fcntl(client_sock, F_SETFL, flags | O_NONBLOCK) < 0) {
            LOG_ERROR("fcntl() failed for client socket");
            close(client_sock);
            return -1;
        }

        struct epoll_event event = {0};
        event.events = EPOLLIN;
        event.data.fd = client_sock;
        if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, client_sock, &event) < 0) {
            LOG_ERROR("epoll_ctl() failed for client socket");
            close(client_sock);
            return -1;
        }

        // Initialize client
        Client* new_client = &ctx->clients[ctx->client_count];
        new_client->sock = client_sock;
        new_client->username[0] = '\0';
        strncpy(new_client->username, "User", sizeof(new_client->username) - 1);
        new_client->username[sizeof(new_client->username) - 1] = '\0';
        new_client->role = ROLE_PARTICIPANT;
        new_client->is_taking_quiz = false;
        new_client->has_submitted = false;
        new_client->score = 0;
        new_client->total = 0;
        new_client->quiz_start_time = 0;
        new_client->current_question = 0;
        new_client->answers[0] = '\0';
        new_client->recv_buffer[0] = '\0';
        new_client->recv_len = 0;
        ctx->client_count++;
        printf("[TCP] Client connected: fd=%d (total: %d)\n", client_sock, ctx->client_count);
        LOG_INFO("New client connected");
        
        // Log to file
        char log_entry[256];
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        snprintf(log_entry, sizeof(log_entry), "[%s] CLIENT_CONNECT fd=%d", timestamp, client_sock);
        storage_save_log(log_entry);
    }
    return client_sock;
}

int net_send_to_client(int sock, const char* data, size_t len) {
    // Add newline delimiter if not present
    char buffer[BUFFER_SIZE + 64];
    size_t total_len = len;
    const char* send_data = data;
    
    if (len < sizeof(buffer) - 2 && (len == 0 || data[len-1] != '\n')) {
        memcpy(buffer, data, len);
        buffer[len] = '\n';
        buffer[len+1] = '\0';
        total_len = len + 1;
        send_data = buffer;
    }

    // Send with partial write handling
    size_t sent = 0;
    while (sent < total_len) {
        int result = send(sock, send_data + sent, total_len - sent, 0);
        if (result > 0) {
            sent += result;
        } else if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, continue later
                usleep(1000);
                continue;
            }
            printf("[TCP] SEND ERROR fd=%d: %s\n", sock, strerror(errno));
            return -1;
        } else {
            break;
        }
    }
    if (sent > 0) {
        printf("[TCP] SEND fd=%d: %.*s\n", sock, (int)(sent > 100 ? 100 : sent), send_data);
    }
    return sent;
}

int net_receive_from_client(int sock, char* buffer, size_t len) {
    int result = recv(sock, buffer, len, 0);
    if (result > 0) {
        buffer[result] = '\0';  // Null terminate for safety
        printf("[TCP] RECV fd=%d: %.*s\n", sock, result, buffer);
    } else if (result == 0) {
        printf("[TCP] Client disconnected: fd=%d\n", sock);
    } else if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        printf("[TCP] RECV ERROR fd=%d: %s\n", sock, strerror(errno));
    }
    return result;
}

void net_close_client(ServerContext* ctx, int sock) {
    printf("[TCP] Closing client connection: fd=%d\n", sock);
    
    // Log disconnect
    storage_log_with_timestamp("CLIENT_DISCONNECT fd=%d", sock);
    
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, sock, NULL);
    close(sock);
    
    // Remove from clients array
    int found_idx = -1;
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].sock == sock) {
            found_idx = i;
            break;
        }
    }
    
    if (found_idx >= 0) {
        // Cleanup upload buffer if active
        if (ctx->upload_buffers[found_idx].active && ctx->upload_buffers[found_idx].data) {
            free(ctx->upload_buffers[found_idx].data);
            ctx->upload_buffers[found_idx].data = NULL;
            ctx->upload_buffers[found_idx].active = false;
        }
        
        // Remove from all rooms
        for (int r = 0; r < ctx->room_count; r++) {
            Room* room = &ctx->rooms[r];
            for (int c = 0; c < room->client_count; c++) {
                if (room->clients[c] && room->clients[c]->sock == sock) {
                    // Shift remaining clients
                    for (int j = c; j < room->client_count - 1; j++) {
                        room->clients[j] = room->clients[j+1];
                    }
                    room->client_count--;
                    break;
                }
            }
        }
        
        // Shift remaining clients in main array
        for (int i = found_idx; i < ctx->client_count - 1; i++) {
            ctx->clients[i] = ctx->clients[i+1];
            // Also shift upload buffers
            ctx->upload_buffers[i] = ctx->upload_buffers[i+1];
        }
        ctx->client_count--;
        printf("[TCP] Client removed from array (remaining: %d)\n", ctx->client_count);
    }
}