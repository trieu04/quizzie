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
        if (type_len > 31) type_len = 31; // allow long message types (e.g., LOAD_PRACTICE_QUESTIONS)
        strncpy(type, buffer, type_len);
        type[type_len] = '\0';
        strncpy(data, colon + 1, 999);
        data[999] = '\0';
    } else {
        strncpy(type, buffer, 31);
        type[31] = '\0';
        data[0] = '\0';
    }
}

static Client* find_client_by_fd(ServerContext* ctx, int fd) {
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].sock == fd) return &ctx->clients[i];
    }
    return NULL;
}

static bool client_is_admin(ServerContext* ctx, int fd) {
    Client* c = find_client_by_fd(ctx, fd);
    return c && c->role == ROLE_ADMIN;
}

static void handle_register(ServerContext* ctx, int fd, const char* data) {
    char username[64] = "", password[64] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        size_t ulen = comma - data;
        if (ulen > 63) ulen = 63;
        strncpy(username, data, ulen);
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
}

static void handle_login(ServerContext* ctx, int fd, const char* data) {
    char username[64] = "", password[64] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        size_t ulen = comma - data;
        if (ulen > 63) ulen = 63;
        strncpy(username, data, ulen);
        username[ulen] = '\0';
        strncpy(password, comma + 1, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
    }

    int role = 0;
    if (storage_verify_login(username, password, &role) == 0) {
        Client* c = find_client_by_fd(ctx, fd);
        if (c) {
            strncpy(c->username, username, sizeof(c->username) - 1);
            c->role = role;
        }
        char response[64];
        snprintf(response, sizeof(response), "LOGIN_SUCCESS:%d", role);
        net_send_to_client(fd, response, strlen(response));
    } else {
        net_send_to_client(fd, "LOGIN_FAILED:Invalid credentials", 33);
    }
}

static void handle_upload_questions(ServerContext* ctx, int fd, const char* data) {
    if (!client_is_admin(ctx, fd)) {
        net_send_to_client(fd, "UPLOAD_FAILED:Admin access required", 36);
        return;
    }

    char filename[128] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        size_t flen = comma - data;
        if (flen > 127) flen = 127;
        strncpy(filename, data, flen);
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

static void handle_create_room(ServerContext* ctx, int fd, const char* data) {
    if (!client_is_admin(ctx, fd)) {
        net_send_to_client(fd, "ERROR:Only admins can create rooms", 35);
        return;
    }

    char username[50] = "";
    char config[256] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        size_t ulen = comma - data;
        if (ulen > 49) ulen = 49;
        strncpy(username, data, ulen);
        username[ulen] = '\0';
        strncpy(config, comma + 1, sizeof(config) - 1);
    } else {
        strncpy(username, data, sizeof(username) - 1);
    }
    room_create(ctx, fd, username, config);
}

static void handle_join_room(ServerContext* ctx, int fd, const char* data) {
    int room_id = 0;
    char username[50] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        *comma = '\0';
        room_id = atoi(data);
        strncpy(username, comma + 1, sizeof(username) - 1);
    } else {
        room_id = atoi(data);
        strcpy(username, "Guest");
    }
    room_join(ctx, fd, room_id, username);
}

static void handle_start_game(ServerContext* ctx, int fd) {
    room_start_quiz(ctx, fd);
}

static void handle_client_start(ServerContext* ctx, int fd) {
    room_client_start_quiz(ctx, fd);
}

static void handle_get_stats(ServerContext* ctx, int fd) {
    room_get_stats(ctx, fd);
}

static void handle_set_config(ServerContext* ctx, int fd, const char* data) {
    room_set_config(ctx, fd, data);
}

static void handle_list_rooms(ServerContext* ctx, int fd) {
    room_list(ctx, fd);
}

static void handle_rejoin_host(ServerContext* ctx, int fd, const char* data) {
    room_rejoin_as_host(ctx, fd, data);
}

static void handle_submit(ServerContext* ctx, int fd, const char* data) {
    room_submit_answers(ctx, fd, data);
}

static void handle_delete_room(ServerContext* ctx, int fd, const char* data) {
    int room_id = atoi(data);
    if (room_delete(ctx, room_id) == 0) {
        net_send_to_client(fd, "ROOM_DELETED:Success", 20);
    } else {
        net_send_to_client(fd, "ERROR:Room not found", 20);
    }
}

static void handle_save_answer(ServerContext* ctx, int fd, const char* data) {
    // Format: SAVE_ANSWER:room_id,question_idx,answer
    int room_id = 0, idx = 0;
    char ans = '-';
    if (sscanf(data, "%d,%d,%c", &room_id, &idx, &ans) >= 2) {
        Client* c = find_client_by_fd(ctx, fd);
        if (c && idx >= 0 && idx < (int)sizeof(c->answers) - 1) {
            c->answers[idx] = ans;
            c->current_question = idx;
        }
    }
}

static void handle_rejoin_participant(ServerContext* ctx, int fd, const char* data) {
    // Format: REJOIN_PARTICIPANT:room_id,username
    int room_id = 0;
    char username[50] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        *comma = '\0';
        room_id = atoi(data);
        strncpy(username, comma + 1, sizeof(username) - 1);
    } else {
        return;
    }

    // Find room and participant
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].id == room_id) {
            Room* room = &ctx->rooms[i];
            for (int j = 0; j < room->client_count; j++) {
                if (room->clients[j] && strcmp(room->clients[j]->username, username) == 0) {
                    // Found participant, update socket
                    Client* c = room->clients[j];
                    c->sock = fd;
                    
                    // Update server client list
                    Client* sc = find_client_by_fd(ctx, fd);
                    if (sc) {
                        strncpy(sc->username, username, sizeof(sc->username) - 1);
                    }
                    
                    // Send rejoin confirmation with state
                    int remaining = 0;
                    if (c->quiz_start_time > 0 && room->quiz_duration > 0) {
                        time_t now = time(NULL);
                        int elapsed = (int)(now - c->quiz_start_time);
                        remaining = room->quiz_duration - elapsed;
                        if (remaining < 0) remaining = 0;
                    }
                    
                    char response[256];
                    snprintf(response, sizeof(response), "REJOINED:%d,%d,%d,%d", 
                             room_id, room->quiz_duration, room->state, remaining);
                    net_send_to_client(fd, response, strlen(response));
                    
                    // If already taking quiz, resend questions and answers
                    if (c->is_taking_quiz && !c->has_submitted) {
                        char q_msg[BUFFER_SIZE];
                        snprintf(q_msg, sizeof(q_msg), "QUESTIONS:%d;%s", remaining, room->questions);
                        net_send_to_client(fd, q_msg, strlen(q_msg));
                        
                        if (strlen(c->answers) > 0) {
                            char ans_msg[128];
                            snprintf(ans_msg, sizeof(ans_msg), "ANSWERS:%s,%d", c->answers, c->current_question);
                            net_send_to_client(fd, ans_msg, strlen(ans_msg));
                        }
                    } else if (c->has_submitted) {
                        // Already submitted, send result
                        char result[64];
                        snprintf(result, sizeof(result), "RESULT:%d/%d,%d", c->score, c->total, 0);
                        net_send_to_client(fd, result, strlen(result));
                    }
                    return;
                }
            }
        }
    }
    net_send_to_client(fd, "ERROR:Participant not found in room", 36);
}

static void handle_load_practice_questions(ServerContext* ctx, int fd, const char* data) {
    (void)ctx;
    // Format: LOAD_PRACTICE_QUESTIONS:subject,easy_count,medium_count,hard_count
    char subject[32] = "";
    int easy_req = 0, med_req = 0, hard_req = 0;
    
    // Parse subject,easy,medium,hard
    int parsed = sscanf(data, "%31[^,],%d,%d,%d", subject, &easy_req, &med_req, &hard_req);
    if (parsed < 4) {
        net_send_to_client(fd, "ERROR:Invalid practice request format", 38);
        return;
    }
    
    // Validate individual counts
    if (easy_req < 0 || med_req < 0 || hard_req < 0) {
        net_send_to_client(fd, "ERROR:Question counts cannot be negative", 40);
        return;
    }
    
    int total_req = easy_req + med_req + hard_req;
    if (total_req <= 0) {
        net_send_to_client(fd, "ERROR:Must request at least 1 question", 39);
        return;
    }
    
    if (total_req > MAX_QUESTIONS) {
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "ERROR:Maximum %d questions allowed (requested %d)", MAX_QUESTIONS, total_req);
        net_send_to_client(fd, err_msg, strlen(err_msg));
        return;
    }
    
    // Load practice bank for subject
    char filename[64];
    snprintf(filename, sizeof(filename), "practice_bank_%s.csv", subject);
    
    char questions[BUFFER_SIZE];
    char answers[MAX_QUESTIONS + 1];
    
    if (storage_load_practice_questions(filename, easy_req, med_req, hard_req, questions, answers) != 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "ERROR:Failed to load practice questions for subject '%s'", subject);
        net_send_to_client(fd, err_msg, strlen(err_msg));
        return;
    }
    
    // Send back practice questions with answers
    char response[BUFFER_SIZE + 64];
    snprintf(response, sizeof(response), "PRACTICE_QUESTIONS:%s,%s", questions, answers);
    net_send_to_client(fd, response, strlen(response));
}

static void dispatch_message(ServerContext* ctx, int fd, const Message* msg) {
    if (strcmp(msg->type, "REGISTER") == 0) {
        handle_register(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "LOGIN") == 0) {
        handle_login(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "UPLOAD_QUESTIONS") == 0) {
        handle_upload_questions(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "CREATE_ROOM") == 0) {
        handle_create_room(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "JOIN_ROOM") == 0) {
        handle_join_room(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "START_GAME") == 0) {
        handle_start_game(ctx, fd);
    } else if (strcmp(msg->type, "CLIENT_START") == 0) {
        handle_client_start(ctx, fd);
    } else if (strcmp(msg->type, "GET_STATS") == 0) {
        handle_get_stats(ctx, fd);
    } else if (strcmp(msg->type, "SET_CONFIG") == 0) {
        handle_set_config(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "LIST_ROOMS") == 0) {
        handle_list_rooms(ctx, fd);
    } else if (strcmp(msg->type, "REJOIN_HOST") == 0) {
        handle_rejoin_host(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "SUBMIT") == 0) {
        handle_submit(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "DELETE_ROOM") == 0) {
        handle_delete_room(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "SAVE_ANSWER") == 0) {
        handle_save_answer(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "REJOIN_PARTICIPANT") == 0) {
        handle_rejoin_participant(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "LOAD_PRACTICE_QUESTIONS") == 0) {
        handle_load_practice_questions(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "GET_QUESTION_FILES") == 0) {
        char buffer[4096];
        if (storage_get_question_files(buffer, sizeof(buffer)) == 0) {
            char response[4200];
            snprintf(response, sizeof(response), "QUESTION_FILES:%s", buffer);
            net_send_to_client(fd, response, strlen(response));
        } else {
            net_send_to_client(fd, "ERROR:Failed to list files", 24);
        }
    }
}

void server_run(ServerContext* ctx) {
    // All questions are loaded from data/questions/ CSVs by room (exam) or client (practice).
    // No server-side default question loading needed.

    // Setup network
    if (net_setup(ctx) != 0) {
        LOG_ERROR("Failed to setup network");
        return;
    }

    LOG_INFO("Server started");

    struct epoll_event events[MAX_CLIENTS];
    time_t last_save_time = time(NULL);

    while (ctx->running) {
        // Autosave every 10 seconds
        time_t now = time(NULL);
        if (now - last_save_time >= 10) {
            storage_save_server_state(ctx);
            last_save_time = now;
            // LOG_INFO("Autosaved server state"); // constant logging might be spammy, maybe only on error?
        }

        int event_count = epoll_wait(ctx->epoll_fd, events, MAX_CLIENTS, 1000); // 1s timeout
        for (int i = 0; i < event_count; i++) {
            int fd = events[i].data.fd;
            if (fd == ctx->server_fd) {
                net_accept_client(ctx);
            } else {
                // Find client buffer
                Client* client = find_client_by_fd(ctx, fd);
                if (!client) {
                    net_close_client(ctx, fd);
                    continue;
                }
                
                char temp_buf[BUFFER_SIZE];
                int len = net_receive_from_client(fd, temp_buf, BUFFER_SIZE - 1);
                
                if (len > 0) {
                    temp_buf[len] = '\0';
                    
                    // Append to client's receive buffer
                    if (client->recv_len + len < (int)sizeof(client->recv_buffer) - 1) {
                        memcpy(client->recv_buffer + client->recv_len, temp_buf, len);
                        client->recv_len += len;
                        client->recv_buffer[client->recv_len] = '\0';
                    } else {
                        // Buffer overflow, reset
                        printf("[TCP] Buffer overflow for fd=%d, resetting\n", fd);
                        client->recv_len = 0;
                        client->recv_buffer[0] = '\0';
                        continue;
                    }
                    
                    // Parse complete messages (delimited by \n)
                    char* line_start = client->recv_buffer;
                    char* newline;
                    while ((newline = strchr(line_start, '\n')) != NULL) {
                        *newline = '\0';  // Terminate the line
                        
                        if (strlen(line_start) > 0) {
                            // Parse and handle message
                            Message msg;
                            memset(&msg, 0, sizeof(msg));
                            parse_message(line_start, msg.type, msg.data);
                            
                            printf("[TCP] Processing message type: %s from fd=%d\n", msg.type, fd);
                            dispatch_message(ctx, fd, &msg);
                        }
                        
                        line_start = newline + 1;
                    }
                    
                    // Move remaining incomplete data to start of buffer
                    int remaining = strlen(line_start);
                    if (remaining > 0) {
                        memmove(client->recv_buffer, line_start, remaining + 1);
                        client->recv_len = remaining;
                    } else {
                        client->recv_buffer[0] = '\0';
                        client->recv_len = 0;
                    }
                } else {
                    // Client disconnect
                    net_close_client(ctx, fd);
                }
            }
        }
    }
}