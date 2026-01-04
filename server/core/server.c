#include "../include/server.h"
#include "../include/net.h"
#include "../include/storage.h"
#include "../include/room.h"
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
    ctx->next_room_id = 1;
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

// Parse message with format "TYPE:DATA" where DATA can contain colons
static void parse_message(const char* buffer, char* type, char* data) {
    const char* colon = strchr(buffer, ':');
    if (colon) {
        size_t type_len = colon - buffer;
        if (type_len > 19) type_len = 19;
        strncpy(type, buffer, type_len);
        type[type_len] = '\0';
        strncpy(data, colon + 1, 999);
        data[999] = '\0';
    } else {
        strncpy(type, buffer, 19);
        type[19] = '\0';
        data[0] = '\0';
    }
}

void server_run(ServerContext* ctx) {
    // Load questions: try several likely relative paths so running from
    // different working directories still finds the file.
    char questions[BUFFER_SIZE], answers[50];
    const char* candidates[] = {
        "data/questions.txt",
        "../data/questions.txt",
        "../data/questions.txt",
        "../../data/questions.txt",
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
                int len = net_receive_from_client(fd, buffer, BUFFER_SIZE - 1);
                // Handle received data
                if (len > 0) {
                    buffer[len] = '\0';
                    
                    // Parse and handle message
                    Message msg;
                    memset(&msg, 0, sizeof(msg));
                    parse_message(buffer, msg.type, msg.data);
                    
                    printf("[TCP] Processing message type: %s from fd=%d\n", msg.type, fd);
                    
                    if (strcmp(msg.type, "REGISTER") == 0) {
                        // Format: REGISTER:username,password
                        char username[64] = "", password[64] = "";
                        char* comma = strchr(msg.data, ',');
                        if (comma) {
                            size_t ulen = comma - msg.data;
                            if (ulen > 63) ulen = 63;
                            strncpy(username, msg.data, ulen);
                            username[ulen] = '\0';
                            strncpy(password, comma + 1, sizeof(password) - 1);
                            password[sizeof(password) - 1] = '\0';
                        }
                        
                        int result = storage_register_user(username, password);
                        if (result == 0) {
                            net_send_to_client(fd, "REGISTER_SUCCESS:Account created", 33);
                        } else if (result == -2) {
                            net_send_to_client(fd, "REGISTER_FAILED:Username already exists", 40);
                        } else {
                            net_send_to_client(fd, "REGISTER_FAILED:Registration error", 35);
                        }
                    } else if (strcmp(msg.type, "LOGIN") == 0) {
                        // Format: LOGIN:username,password
                        char username[64] = "", password[64] = "";
                        char* comma = strchr(msg.data, ',');
                        if (comma) {
                            size_t ulen = comma - msg.data;
                            if (ulen > 63) ulen = 63;
                            strncpy(username, msg.data, ulen);
                            username[ulen] = '\0';
                            strncpy(password, comma + 1, sizeof(password) - 1);
                            password[sizeof(password) - 1] = '\0';
                        }
                        
                        int role = 0;
                        if (storage_verify_login(username, password, &role) == 0) {
                            // Update client username and role
                            for (int j = 0; j < ctx->client_count; j++) {
                                if (ctx->clients[j].sock == fd) {
                                    strncpy(ctx->clients[j].username, username, sizeof(ctx->clients[j].username) - 1);
                                    ctx->clients[j].role = role;
                                    break;
                                }
                            }
                            char response[64];
                            snprintf(response, sizeof(response), "LOGIN_SUCCESS:%d", role);
                            net_send_to_client(fd, response, strlen(response));
                        } else {
                            net_send_to_client(fd, "LOGIN_FAILED:Invalid credentials", 33);
                        }
                    } else if (strcmp(msg.type, "UPLOAD_QUESTIONS") == 0) {
                        // Format: UPLOAD_QUESTIONS:filename,csv_data
                        // Check if user is admin
                        int is_admin = 0;
                        for (int j = 0; j < ctx->client_count; j++) {
                            if (ctx->clients[j].sock == fd && ctx->clients[j].role == 1) {
                                is_admin = 1;
                                break;
                            }
                        }
                        
                        if (!is_admin) {
                            net_send_to_client(fd, "UPLOAD_FAILED:Admin access required", 36);
                        } else {
                            char filename[128] = "";
                            char* comma = strchr(msg.data, ',');
                            if (comma) {
                                size_t flen = comma - msg.data;
                                if (flen > 127) flen = 127;
                                strncpy(filename, msg.data, flen);
                                filename[flen] = '\0';
                                const char* csv_data = comma + 1;
                                
                                if (storage_save_csv_bank(filename, csv_data) == 0) {
                                    net_send_to_client(fd, "UPLOAD_SUCCESS:CSV saved", 24);
                                } else {
                                    net_send_to_client(fd, "UPLOAD_FAILED:Failed to save", 29);
                                }
                            } else {
                                net_send_to_client(fd, "UPLOAD_FAILED:Invalid format", 29);
                            }
                        }
                    } else if (strcmp(msg.type, "CREATE_ROOM") == 0) {
                        // Check if user is admin
                        int is_admin = 0;
                        for (int j = 0; j < ctx->client_count; j++) {
                            if (ctx->clients[j].sock == fd && ctx->clients[j].role == 1) {
                                is_admin = 1;
                                break;
                            }
                        }
                        
                        if (!is_admin) {
                            net_send_to_client(fd, "ERROR:Only admins can create rooms", 35);
                        } else {
                            // Format: CREATE_ROOM:username,duration,filename or CREATE_ROOM:username
                            char username[50] = "";
                            char config[256] = "";
                            char* comma = strchr(msg.data, ',');
                            if (comma) {
                                size_t ulen = comma - msg.data;
                                if (ulen > 49) ulen = 49;
                                strncpy(username, msg.data, ulen);
                                username[ulen] = '\0';
                                strncpy(config, comma + 1, sizeof(config) - 1);
                            } else {
                                strncpy(username, msg.data, sizeof(username) - 1);
                            }
                            room_create(ctx, fd, username, config);
                        }
                    } else if (strcmp(msg.type, "JOIN_ROOM") == 0) {
                        // Format: JOIN_ROOM:room_id,username
                        int room_id = 0;
                        char username[50] = "";
                        char* comma = strchr(msg.data, ',');
                        if (comma) {
                            *comma = '\0';
                            room_id = atoi(msg.data);
                            strncpy(username, comma + 1, sizeof(username) - 1);
                        } else {
                            room_id = atoi(msg.data);
                            strcpy(username, "Guest");
                        }
                        room_join(ctx, fd, room_id, username);
                    } else if (strcmp(msg.type, "START_GAME") == 0) {
                        room_start_quiz(ctx, fd);
                    } else if (strcmp(msg.type, "CLIENT_START") == 0) {
                        room_client_start_quiz(ctx, fd);
                    } else if (strcmp(msg.type, "GET_STATS") == 0) {
                        room_get_stats(ctx, fd);
                    } else if (strcmp(msg.type, "SET_CONFIG") == 0) {
                        room_set_config(ctx, fd, msg.data);
                    } else if (strcmp(msg.type, "LIST_ROOMS") == 0) {
                        room_list(ctx, fd);
                    } else if (strcmp(msg.type, "REJOIN_HOST") == 0) {
                        room_rejoin_as_host(ctx, fd, msg.data);
                    } else if (strcmp(msg.type, "SUBMIT") == 0) {
                        room_submit_answers(ctx, fd, msg.data);
                    } else if (strcmp(msg.type, "DELETE_ROOM") == 0) {
                        int room_id = atoi(msg.data);
                        if (room_delete(ctx, room_id) == 0) {
                            net_send_to_client(fd, "ROOM_DELETED:Success", 20);
                        } else {
                            net_send_to_client(fd, "ERROR:Room not found", 20);
                        }
                    }
                } else {
                    // Client disconnect
                    net_close_client(ctx, fd);
                }
            }
        }
    }
}