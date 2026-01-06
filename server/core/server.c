#include "../include/server.h"
#include "../include/net.h"
#include "../include/storage.h"
#include "../include/room.h"
#include <sys/epoll.h>

// Message parsing constants
#define MAX_MESSAGE_TYPE_LEN 32
#define MAX_MESSAGE_DATA_LEN 1000

// Helper function to find client index by fd
static int find_client_index(ServerContext* ctx, int fd) {
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].sock == fd) {
            return i;
        }
    }
    return -1;
}

// Helper function to send error message
static void send_error(int fd, const char* message) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "ERROR:%s", message);
    net_send_to_client(fd, buffer, strlen(buffer));
}

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
    
    // Initialize upload buffers
    for (int i = 0; i < MAX_CLIENTS; i++) {
        ctx->upload_buffers[i].active = false;
        ctx->upload_buffers[i].data = NULL;
        ctx->upload_buffers[i].current_size = 0;
        ctx->upload_buffers[i].capacity = 0;
        ctx->upload_buffers[i].total_chunks = 0;
        ctx->upload_buffers[i].received_chunks = 0;
    }
    
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
        if (type_len >= MAX_MESSAGE_TYPE_LEN) type_len = MAX_MESSAGE_TYPE_LEN - 1;
        strncpy(type, buffer, type_len);
        type[type_len] = '\0';
        strncpy(data, colon + 1, MAX_MESSAGE_DATA_LEN - 1);
        data[MAX_MESSAGE_DATA_LEN - 1] = '\0';
    } else {
        strncpy(type, buffer, MAX_MESSAGE_TYPE_LEN - 1);
        type[MAX_MESSAGE_TYPE_LEN - 1] = '\0';
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
    (void)ctx; // Context not needed for registration
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
    
    // Validate username length
    if (strlen(username) < 3) {
        net_send_to_client(fd, "REGISTER_FAILED:Username must be at least 3 characters", 55);
        return;
    }
    
    // Validate password length
    if (strlen(password) < 6) {
        net_send_to_client(fd, "REGISTER_FAILED:Password must be at least 6 characters", 55);
        return;
    }
    
    // Validate username contains only alphanumeric and underscore
    for (size_t i = 0; i < strlen(username); i++) {
        char c = username[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            net_send_to_client(fd, "REGISTER_FAILED:Username can only contain letters, numbers and underscore", 74);
            return;
        }
    }

    int result = storage_register_user(username, password);
    if (result == 0) {
        net_send_to_client(fd, "REGISTER_SUCCESS:Account created successfully", 46);
    } else if (result == -2) {
        net_send_to_client(fd, "REGISTER_FAILED:Username already exists", 40);
    } else {
        net_send_to_client(fd, "REGISTER_FAILED:Registration error. Please try again", 53);
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
    int login_result = storage_verify_login(username, password, &role);
    if (login_result == 0) {
        // Check if this username is already logged in elsewhere
        for (int i = 0; i < ctx->client_count; i++) {
            if (ctx->clients[i].sock >= 0 && ctx->clients[i].sock != fd && 
                strlen(ctx->clients[i].username) > 0 &&
                strcmp(ctx->clients[i].username, username) == 0) {
                // Found another active session with same username
                // Send logout notification to old session
                net_send_to_client(ctx->clients[i].sock, "FORCE_LOGOUT:Another user logged in with your account", 55);
                // Close the old connection
                net_close_client(ctx, ctx->clients[i].sock);
                break;
            }
        }

        Client* c = find_client_by_fd(ctx, fd);
        if (c) {
            strncpy(c->username, username, sizeof(c->username) - 1);
            c->role = role;
        }
        char response[64];
        snprintf(response, sizeof(response), "LOGIN_SUCCESS:%d", role);
        net_send_to_client(fd, response, strlen(response));
    } else {
        // Send same message for both wrong password and non-existent user for security
        net_send_to_client(fd, "LOGIN_FAILED:Username or password is wrong, disconnect", 55);
        // Disconnect client after failed login
        net_close_client(ctx, fd);
    }
}

static void handle_upload_start(ServerContext* ctx, int fd, const char* data) {
    if (!client_is_admin(ctx, fd)) {
        send_error(fd, "Admin access required");
        return;
    }

    char filename[MAX_FILENAME_LEN] = "";
    int total_chunks = 0;
    char* comma = strchr(data, ',');
    if (comma) {
        size_t flen = comma - data;
        if (flen > 127) flen = 127;
        strncpy(filename, data, flen);
        filename[flen] = '\0';
        total_chunks = atoi(comma + 1);

        int client_idx = find_client_index(ctx, fd);
        if (client_idx >= 0) {
            UploadBuffer* buf = &ctx->upload_buffers[client_idx];
            
            // Free old buffer if exists
            if (buf->active && buf->data) {
                free(buf->data);
            }

            // Initialize new buffer
            buf->capacity = total_chunks * 1000; // Estimate
            buf->data = (char*)malloc(buf->capacity);
            if (buf->data) {
                buf->data[0] = '\0';
                strncpy(buf->filename, filename, sizeof(buf->filename) - 1);
                buf->filename[sizeof(buf->filename) - 1] = '\0';
                buf->current_size = 0;
                buf->total_chunks = total_chunks;
                buf->received_chunks = 0;
                buf->active = true;
                net_send_to_client(fd, "UPLOAD_READY:Ready to receive chunks", 37);
            } else {
                net_send_to_client(fd, "UPLOAD_FAILED:Memory allocation failed", 39);
            }
        }
    } else {
        net_send_to_client(fd, "UPLOAD_FAILED:Invalid format", 29);
    }
}

static void handle_upload_chunk(ServerContext* ctx, int fd, const char* data) {
    if (!client_is_admin(ctx, fd)) {
        return;
    }

    char* comma = strchr(data, ',');
    if (comma) {
        const char* chunk_data = comma + 1;

        int client_idx = find_client_index(ctx, fd);
        if (client_idx >= 0) {
            UploadBuffer* buf = &ctx->upload_buffers[client_idx];
            if (buf->active && buf->data) {
                size_t chunk_len = strlen(chunk_data);
                size_t new_size = buf->current_size + chunk_len;

                // Resize if needed
                if (new_size >= buf->capacity) {
                    size_t new_capacity = buf->capacity * 2;
                    char* new_data = (char*)realloc(buf->data, new_capacity);
                    if (new_data) {
                        buf->data = new_data;
                        buf->capacity = new_capacity;
                    } else {
                        net_send_to_client(fd, "UPLOAD_FAILED:Memory reallocation failed", 41);
                        return;
                    }
                }

                // Append chunk
                strncat(buf->data, chunk_data, chunk_len);
                buf->current_size = new_size;
                buf->received_chunks++;
            }
        }
    }
}

static void handle_upload_end(ServerContext* ctx, int fd, const char* data) {
    if (!client_is_admin(ctx, fd)) {
        send_error(fd, "Admin access required");
        return;
    }
    (void)data; // Unused parameter

    int client_idx = find_client_index(ctx, fd);
    if (client_idx >= 0) {
        UploadBuffer* buf = &ctx->upload_buffers[client_idx];
        if (buf->active && buf->data) {
            // Verify all chunks received
            if (buf->received_chunks == buf->total_chunks) {
                // Save to file
                if (storage_save_csv_bank(buf->filename, buf->data) == 0) {
                    char success_msg[256];
                    snprintf(success_msg, sizeof(success_msg), 
                            "UPLOAD_SUCCESS:CSV saved (%d chunks, %zu bytes)", 
                            buf->received_chunks, buf->current_size);
                    net_send_to_client(fd, success_msg, strlen(success_msg));
                } else {
                    net_send_to_client(fd, "UPLOAD_FAILED:Failed to save file", 34);
                }
            } else {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), 
                        "UPLOAD_FAILED:Missing chunks (%d/%d)", 
                        buf->received_chunks, buf->total_chunks);
                net_send_to_client(fd, error_msg, strlen(error_msg));
            }

            // Cleanup buffer
            free(buf->data);
            buf->data = NULL;
            buf->active = false;
        } else {
            net_send_to_client(fd, "UPLOAD_FAILED:No active upload", 31);
        }
    }
}

static void handle_create_room(ServerContext* ctx, int fd, const char* data) {
    if (!client_is_admin(ctx, fd)) {
        send_error(fd, "Admin access required to create rooms");
        return;
    }

    char username[MAX_USERNAME_LEN] = "";
    char config[256] = "";
    char* comma = strchr(data, ',');
    if (comma) {
        size_t ulen = comma - data;
        if (ulen > 49) ulen = 49;
        strncpy(username, data, ulen);
        username[ulen] = '\0';
        strncpy(config, comma + 1, sizeof(config) - 1);
        config[sizeof(config) - 1] = '\0';
    } else {
        strncpy(username, data, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    }
    room_create(ctx, fd, username, config);
}

static void handle_join_room(ServerContext* ctx, int fd, const char* data) {
    int room_id = 0;
    char username[MAX_USERNAME_LEN] = "";
    const char* comma = strchr(data, ',');
    if (comma) {
        size_t id_len = comma - data;
        char room_id_str[32];
        if (id_len > 31) id_len = 31;
        strncpy(room_id_str, data, id_len);
        room_id_str[id_len] = '\0';
        room_id = atoi(room_id_str);
        strncpy(username, comma + 1, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
    } else {
        room_id = atoi(data);
        strncpy(username, "Guest", sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
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
        char success_msg[64];
        snprintf(success_msg, sizeof(success_msg), "ROOM_DELETED:%d", room_id);
        net_send_to_client(fd, success_msg, strlen(success_msg));
    } else {
        send_error(fd, "Room not found");
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
    char username[MAX_USERNAME_LEN] = "";
    const char* comma = strchr(data, ',');
    if (comma) {
        size_t id_len = comma - data;
        char room_id_str[32];
        if (id_len > 31) id_len = 31;
        strncpy(room_id_str, data, id_len);
        room_id_str[id_len] = '\0';
        room_id = atoi(room_id_str);
        strncpy(username, comma + 1, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
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
                        int qmsg_len = snprintf(q_msg, sizeof(q_msg), "QUESTIONS:%d;%.32700s", remaining, room->questions);
                        if (qmsg_len > 0 && (size_t)qmsg_len < sizeof(q_msg)) {
                            net_send_to_client(fd, q_msg, strlen(q_msg));
                        }
                        
                        if (strlen(c->answers) > 0) {
                            char ans_msg[128];
                            snprintf(ans_msg, sizeof(ans_msg), "ANSWERS:%.100s,%d", c->answers, c->current_question);
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
    send_error(fd, "Participant not found in room");
}

static void handle_load_practice_questions(ServerContext* ctx, int fd, const char* data) {
    (void)ctx;
    // Format: LOAD_PRACTICE_QUESTIONS:subject,easy_count,medium_count,hard_count
    char subject[32] = "";
    int easy_req = 0, med_req = 0, hard_req = 0;
    
    // Parse subject,easy,medium,hard
    int parsed = sscanf(data, "%31[^,],%d,%d,%d", subject, &easy_req, &med_req, &hard_req);
    if (parsed < 4) {
        send_error(fd, "Invalid practice request format");
        return;
    }
    
    // Validate individual counts
    if (easy_req < 0 || med_req < 0 || hard_req < 0) {
        send_error(fd, "Question counts cannot be negative");
        return;
    }
    
    int total_req = easy_req + med_req + hard_req;
    if (total_req <= 0) {
        send_error(fd, "Must request at least 1 question");
        return;
    }
    
    if (total_req > MAX_QUESTIONS) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Maximum %d questions allowed (requested %d)", MAX_QUESTIONS, total_req);
        send_error(fd, err_msg);
        return;
    }
    
    // Check available questions before loading
    char filename[64];
    snprintf(filename, sizeof(filename), "%s.csv", subject);
    
    // Count available questions
    int available_easy = 0, available_med = 0, available_hard = 0, available_total = 0;
    {
        const char* paths[] = {"data/questions/practice/", "../data/questions/practice/", 
                               "../../data/questions/practice/", NULL};
        FILE* f = NULL;
        char filepath[512];
        for (int i = 0; paths[i]; i++) {
            snprintf(filepath, sizeof(filepath), "%s%s", paths[i], filename);
            f = fopen(filepath, "r");
            if (f) break;
        }
        
        if (f) {
            char line[2048];
            while (fgets(line, sizeof(line), f)) {
                line[strcspn(line, "\n\r")] = 0;
                if (line[0] == '\0' || line[0] == 'i') continue;
                
                char* last_comma = strrchr(line, ',');
                if (last_comma) {
                    char* diff = last_comma + 1;
                    while (*diff == ' ' || *diff == '\t') diff++;
                    
                    if (strncmp(diff, "easy", 4) == 0) available_easy++;
                    else if (strncmp(diff, "hard", 4) == 0) available_hard++;
                    else if (strncmp(diff, "medium", 6) == 0) available_med++;
                }
            }
            fclose(f);
            available_total = available_easy + available_med + available_hard;
        }
    }
    
    // Validate requests against available
    if (easy_req > available_easy || med_req > available_med || hard_req > available_hard) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), 
                 "Not enough questions. Available: Easy=%d Medium=%d Hard=%d. Requested: Easy=%d Medium=%d Hard=%d",
                 available_easy, available_med, available_hard, easy_req, med_req, hard_req);
        send_error(fd, err_msg);
        return;
    }
    
    if (available_total == 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "No questions available for subject '%s'", subject);
        send_error(fd, err_msg);
        return;
    }
    
    // Load practice bank for subject (simple filename format)
    char questions[BUFFER_SIZE];
    char answers[MAX_QUESTIONS + 1];
    
    if (storage_load_practice_questions(filename, easy_req, med_req, hard_req, questions, answers) != 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Failed to load practice questions for subject '%s'", subject);
        send_error(fd, err_msg);
        return;
    }
    
    // Send back practice questions with answers
    char response[BUFFER_SIZE + 128];
    int resp_len = snprintf(response, sizeof(response), "PRACTICE_QUESTIONS:%.32700s,%.100s", questions, answers);
    if (resp_len > 0 && (size_t)resp_len < sizeof(response)) {
        net_send_to_client(fd, response, strlen(response));
    }
}

static void dispatch_message(ServerContext* ctx, int fd, const Message* msg) {
    if (strcmp(msg->type, "REGISTER") == 0) {
        handle_register(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "LOGIN") == 0) {
        handle_login(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "UPLOAD_START") == 0) {
        handle_upload_start(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "UPLOAD_CHUNK") == 0) {
        handle_upload_chunk(ctx, fd, msg->data);
    } else if (strcmp(msg->type, "UPLOAD_END") == 0) {
        handle_upload_end(ctx, fd, msg->data);
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
            send_error(fd, "Failed to list question files");
        }
    } else if (strcmp(msg->type, "GET_PRACTICE_SUBJECTS") == 0) {
        char buffer[1024];
        if (storage_get_practice_subjects(buffer, sizeof(buffer)) == 0) {
            char response[1100];
            snprintf(response, sizeof(response), "PRACTICE_SUBJECTS:%s", buffer);
            net_send_to_client(fd, response, strlen(response));
        } else {
            send_error(fd, "Failed to list practice subjects");
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
    time_t last_timer_check = time(NULL);

    while (ctx->running) {
        // Autosave every 10 seconds
        time_t now = time(NULL);
        if (now - last_save_time >= 10) {
            storage_save_server_state(ctx);
            last_save_time = now;
            // LOG_INFO("Autosaved server state"); // constant logging might be spammy, maybe only on error?
        }
        
        // Check room timers every second
        if (now - last_timer_check >= 1) {
            room_check_timers(ctx);
            last_timer_check = now;
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