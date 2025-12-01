#include "../../include/server.h"
#include "../../include/net.h"
#include "../../include/storage.h"
#include "../../include/room.h"
#include <sys/epoll.h>

ServerContext* server_init() {
    ServerContext* ctx = (ServerContext*)malloc(sizeof(ServerContext));
    if (!ctx) {
        perror("malloc");
        return NULL;
    }
    ctx->server_fd = -1;
    ctx->epoll_fd = -1;
    ctx->client_count = 0;
    ctx->room_count = 0;
    ctx->running = true;
    return ctx;
}

void server_cleanup(ServerContext* ctx) {
    if (ctx) {
        if (ctx->epoll_fd != -1) close(ctx->epoll_fd);
        if (ctx->server_fd != -1) close(ctx->server_fd);
        free(ctx);
    }
}

void server_run(ServerContext* ctx) {
    // Load questions: try several likely relative paths so running from
    // different working directories still finds the file.
    char questions[BUFFER_SIZE], answers[50];
    const char* candidates[] = {
        "data/questions.txt",
        "../data/questions.txt",
        "../../data/questions.txt",
        "../../../data/questions.txt",
        NULL
    };
    int loaded = 0;
    for (int i = 0; candidates[i] != NULL; ++i) {
        if (storage_load_questions(candidates[i], questions, answers) == 0) {
            LOG_INFO("Loaded questions from");
            LOG_INFO(candidates[i]);
            loaded = 1;
            break;
        }
    }
    if (!loaded) {
        LOG_ERROR("Failed to load questions from any candidate path");
        return;
    }

    // Setup network
    if (net_setup(ctx) != 0) {
        LOG_ERROR("Failed to setup network");
        return;
    }

    LOG_INFO("Server started");

    struct epoll_event events[MAX_CLIENTS];
    while (ctx->running) {
        int event_count = epoll_wait(ctx->epoll_fd, events, MAX_CLIENTS, -1);
        for (int i = 0; i < event_count; i++) {
            int fd = events[i].data.fd;
            if (fd == ctx->server_fd) {
                net_accept_client(ctx);
            } else {
                char buffer[BUFFER_SIZE];
                int len = net_receive_from_client(fd, buffer, BUFFER_SIZE);
                if (len > 0) {
                    // Parse and handle message
                    Message msg;
                    sscanf(buffer, "%[^:]:%s", msg.type, msg.data);
                    if (strcmp(msg.type, "CREATE_ROOM") == 0) {
                        room_create(ctx, fd);
                    } else if (strcmp(msg.type, "JOIN_ROOM") == 0) {
                        room_join(ctx, fd, atoi(msg.data));
                    } else if (strcmp(msg.type, "START_GAME") == 0) {
                        room_start_quiz(ctx, fd);
                    } else if (strcmp(msg.type, "SUBMIT") == 0) {
                        room_submit_answers(ctx, fd, msg.data);
                    }
                } else {
                    // Client disconnect
                    net_close_client(ctx, fd);
                }
            }
        }
    }
}