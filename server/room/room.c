#include "../include/room.h"
#include "../include/storage.h"
#include "../include/net.h"
#include <dirent.h>
#include <time.h>
#include <stdio.h>

// Helper function to find a client by socket in a room
static Client* find_client_in_room(Room* room, int sock) {
    for (int i = 0; i < room->client_count; i++) {
        if (room->clients[i] && room->clients[i]->sock == sock) {
            return room->clients[i];
        }
    }
    return NULL;
}

// Helper function to find client by socket in server context
static Client* find_client(ServerContext* ctx, int sock) {
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].sock == sock) {
            return &ctx->clients[i];
        }
    }
    return NULL;
}

// Find room by host username
Room* room_find_by_host_username(ServerContext* ctx, const char* username) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (strcmp(ctx->rooms[i].host_username, username) == 0) {
            return &ctx->rooms[i];
        }
    }
    return NULL;
}

// Find room containing a client socket
static Room* find_room_by_client(ServerContext* ctx, int sock) {
    for (int i = 0; i < ctx->room_count; i++) {
        for (int j = 0; j < ctx->rooms[i].client_count; j++) {
            if (ctx->rooms[i].clients[j] && ctx->rooms[i].clients[j]->sock == sock) {
                return &ctx->rooms[i];
            }
        }
    }
    return NULL;
}

// Removed unused helper room_list_question_files to avoid unused warnings

// List all rooms
void room_list(ServerContext* ctx, int client_sock) {
    char response[BUFFER_SIZE] = "ROOM_LIST:";
    int first = 1;
    
    for (int i = 0; i < ctx->room_count; i++) {
        Room* room = &ctx->rooms[i];
        if (!first) strcat(response, ";");
        
        char room_info[64];
        int participant_count = room->client_count;
        sprintf(room_info, "%d,%s,%d,%d", room->id, room->host_username, 
                participant_count, room->state);
        strcat(response, room_info);
        first = 0;
    }
    
    net_send_to_client(client_sock, response, strlen(response));
}

int room_create(ServerContext* ctx, int host_sock, const char* username, const char* config) {
    if (ctx->room_count >= MAX_ROOMS) {
        net_send_to_client(host_sock, "ERROR:Max rooms reached", 23);
        return -1;
    }

    // Check if user already has a room - delete the old one first
    Room* existing = room_find_by_host_username(ctx, username);
    if (existing) {
        room_delete(ctx, existing->id);
    }

    Room* room = &ctx->rooms[ctx->room_count];
    room->id = ctx->next_room_id++;  // Use unique incremental ID
    room->host_sock = host_sock;
    strncpy(room->host_username, username, sizeof(room->host_username) - 1);
    room->client_count = 0;  // Host is not counted as participant
    room->state = QUIZ_STATE_STARTED; // Auto-start (time-based)
    room->quiz_duration = 300;  // Default 5 minutes
    strcpy(room->question_file, "exam_bank_programming.csv");
    room->start_time = 0;
    room->end_time = 0;
    room->randomize_answers = false;
    
    // Parse config
    // New format: duration|start_time|end_time|filename|easy|med|hard|any|rand_ans
    // Legacy formats: "duration,filename" or "subject,duration" etc.
    
    bool loaded = false;
    
    if (config && strchr(config, '|')) {
        // New format
        char config_copy[512];
        strncpy(config_copy, config, sizeof(config_copy) - 1);
        config_copy[sizeof(config_copy) - 1] = '\0';
        
        char *token = strtok(config_copy, "|");
        if (token) room->quiz_duration = atoi(token);
        
        token = strtok(NULL, "|");
        if (token) room->start_time = atol(token);
        
        token = strtok(NULL, "|");
        if (token) room->end_time = atol(token);
        
        token = strtok(NULL, "|");
        if (token) strncpy(room->question_file, token, sizeof(room->question_file) - 1);

        int easy=0, med=0, hard=0, any=0;
        token = strtok(NULL, "|"); if (token) easy = atoi(token);
        token = strtok(NULL, "|"); if (token) med = atoi(token);
        token = strtok(NULL, "|"); if (token) hard = atoi(token);
        token = strtok(NULL, "|"); if (token) any = atoi(token);
        
        token = strtok(NULL, "|"); 
        if (token) room->randomize_answers = (atoi(token) != 0);
        
        // Load questions
        if (storage_load_filtered_questions(room->question_file, easy, med, hard, any, 
                                            room->randomize_answers, room->questions, room->correct_answers) == 0) {
            loaded = true;
        }
    } else if (config && strlen(config) > 0) {
        // Legacy format fallback
         char config_copy[256];
        strncpy(config_copy, config, sizeof(config_copy) - 1);
        config_copy[sizeof(config_copy) - 1] = '\0';
        char* tok1 = strtok(config_copy, ",");
        char* tok2 = strtok(NULL, ",");
        char* tok3 = strtok(NULL, ",");
        
        if (tok1) {
            // If tok1 is numeric => duration; else it's subject
            bool is_num = true;
            for (int i = 0; tok1[i]; i++) { if (tok1[i] < '0' || tok1[i] > '9') { is_num = false; break; } }
            if (is_num) {
                room->quiz_duration = atoi(tok1);
                if (tok2) {
                    strncpy(room->question_file, tok2, sizeof(room->question_file) - 1);
                }
            } else {
                // subject provided
                char subject[64];
                strncpy(subject, tok1, sizeof(subject) - 1);
                subject[sizeof(subject) - 1] = '\0';
                snprintf(room->question_file, sizeof(room->question_file), "exam_bank_%s.csv", subject);
                if (tok2) {
                    room->quiz_duration = atoi(tok2);
                }
                if (tok3) {
                    strncpy(room->question_file, tok3, sizeof(room->question_file) - 1);
                }
            }
        }
        
        // Legacy load
        char filepath[256];
        const char* prefixes[] = {
            "data/questions/",
            "../data/questions/",
            "../../data/questions/",
            "data/",
            "../data/",
            NULL
        };
        for (int i = 0; prefixes[i] != NULL; ++i) {
            snprintf(filepath, sizeof(filepath), "%s%s", prefixes[i], room->question_file);
            if (storage_load_questions(filepath, room->questions, room->correct_answers) == 0) {
                loaded = true;
                break;
            }
        }
    } else {
        // No config, default load
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "data/questions/%s", room->question_file);
        if (storage_load_questions(filepath, room->questions, room->correct_answers) == 0) loaded = true;
    }

    if (!loaded) {
        LOG_ERROR("Failed to load questions for room");
        net_send_to_client(host_sock, "ERROR:Failed to load questions", 30);
        return -1;
    }

    // Count total questions from correct_answers
    int total_q = 0;
    if (strlen(room->correct_answers) > 0) {
        total_q = 1;
        for (const char* p = room->correct_answers; *p; p++) {
             if (*p == ',') total_q++;
        }
    }

    char response[64];
    snprintf(response, sizeof(response), "ROOM_CREATED:%d,%d,%d", room->id, room->quiz_duration, total_q);
    net_send_to_client(host_sock, response, strlen(response));
    ctx->room_count++;
    LOG_INFO("Room created successfully");
    
    // Log to file
    char log_entry[256];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(log_entry, sizeof(log_entry), "[%s] ROOM_CREATED id=%d host=%s duration=%d", 
             timestamp, room->id, username, room->quiz_duration);
    storage_save_log(log_entry);
    return 0;
}

int room_rejoin_as_host(ServerContext* ctx, int client_sock, const char* username) {
    Room* room = room_find_by_host_username(ctx, username);
    if (!room) {
        net_send_to_client(client_sock, "ERROR:No room found for this user", 33);
        return -1;
    }
    
    room->host_sock = client_sock;
    
    Client* client = find_client(ctx, client_sock);
    if (client) {
        strncpy(client->username, username, sizeof(client->username) - 1);
    }
    
    char response[64];
    snprintf(response, sizeof(response), "ROOM_REJOINED:%d,%d,%d", room->id, room->quiz_duration, room->state);
    net_send_to_client(client_sock, response, strlen(response));
    LOG_INFO("Host rejoined room");
    return 0;
}

int room_set_config(ServerContext* ctx, int host_sock, const char* config) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].host_sock == host_sock) {
            Room* room = &ctx->rooms[i];
            
            if (room->state != QUIZ_STATE_WAITING) {
                net_send_to_client(host_sock, "ERROR:Cannot change config after quiz started", 45);
                return -1;
            }
            
            // Parse config: accepts "duration,filename" or "subject,duration[,filename]"
            char config_copy[256];
            strncpy(config_copy, config, sizeof(config_copy) - 1);
            config_copy[sizeof(config_copy) - 1] = '\0';
            char* tok1 = strtok(config_copy, ",");
            char* tok2 = strtok(NULL, ",");
            char* tok3 = strtok(NULL, ",");
            
            if (tok1) {
                bool is_num = true;
                for (int i = 0; tok1[i]; i++) { if (tok1[i] < '0' || tok1[i] > '9') { is_num = false; break; } }
                if (is_num) {
                    room->quiz_duration = atoi(tok1);
                    if (tok2 && strlen(tok2) > 0) {
                        strncpy(room->question_file, tok2, sizeof(room->question_file) - 1);
                    }
                } else {
                    // subject provided
                    char subject[64];
                    strncpy(subject, tok1, sizeof(subject) - 1);
                    subject[sizeof(subject) - 1] = '\0';
                    snprintf(room->question_file, sizeof(room->question_file), "exam_bank_%s.csv", subject);
                    if (tok2) room->quiz_duration = atoi(tok2);
                    if (tok3 && strlen(tok3) > 0) {
                        strncpy(room->question_file, tok3, sizeof(room->question_file) - 1);
                    }
                }
            }
            
            // Reload questions
            char filepath[256];
            const char* prefixes[] = {
                "data/questions/",
                "../data/questions/",
                "../../data/questions/",
                "data/",
                "../data/",
                NULL
            };
            int loaded = 0;
            for (int j = 0; prefixes[j] != NULL; ++j) {
                snprintf(filepath, sizeof(filepath), "%s%s", prefixes[j], room->question_file);
                if (storage_load_questions(filepath, room->questions, room->correct_answers) == 0) {
                    loaded = 1;
                    break;
                }
            }
            if (!loaded) {
                net_send_to_client(host_sock, "ERROR:Failed to load question file", 34);
                return -1;
            }
            
            char response[256];
            snprintf(response, sizeof(response), "CONFIG_UPDATED:%d,%s", room->quiz_duration, room->question_file);
            net_send_to_client(host_sock, response, strlen(response));
            return 0;
        }
    }
    net_send_to_client(host_sock, "ERROR:Not a room host", 21);
    return -1;
}

int room_join(ServerContext* ctx, int client_sock, int room_id, const char* username) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].id == room_id) {
            Room* room = &ctx->rooms[i];
            
            if (room->client_count >= MAX_CLIENTS) {
                net_send_to_client(client_sock, "ERROR:Room is full", 18);
                return -1;
            }

            Client* client = find_client(ctx, client_sock);
            if (client) {
                // Check if client is already in this room (prevent duplicates)
                for (int j = 0; j < room->client_count; j++) {
                    if (room->clients[j] == client) {
                        // Already in room, just send confirmation
                        char response[64];
                        snprintf(response, sizeof(response), "JOINED:%d,%d,%d", room_id, room->quiz_duration, room->state);
                        net_send_to_client(client_sock, response, strlen(response));
                        return 0;
                    }
                }
                
                strncpy(client->username, username, sizeof(client->username) - 1);
                client->is_taking_quiz = false;
                client->has_submitted = false;
                client->quiz_start_time = 0;
                client->score = 0;
                client->total = 0;
                memset(client->answers, 0, sizeof(client->answers));
                client->current_question = 0;
                
                room->clients[room->client_count] = client;
                room->client_count++;

                char response[64];
                snprintf(response, sizeof(response), "JOINED:%d,%d,%d", room_id, room->quiz_duration, room->state);
                net_send_to_client(client_sock, response, strlen(response));
                
                // Notify host about new participant
                char notify[128];
                snprintf(notify, sizeof(notify), "PARTICIPANT_JOINED:%s", username);
                net_send_to_client(room->host_sock, notify, strlen(notify));
                
                LOG_INFO("Client joined room");
                
                // Log to file
                char log_entry[256];
                time_t now = time(NULL);
                struct tm* tm_info = localtime(&now);
                char ts[64];
                strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
                snprintf(log_entry, sizeof(log_entry), "[%s] ROOM_JOIN room_id=%d user=%s", 
                         ts, room_id, username);
                storage_save_log(log_entry);
                return 0;
            }
        }
    }
    net_send_to_client(client_sock, "ERROR:Room not found", 20);
    return -1;
}

int room_start_quiz(ServerContext* ctx, int host_sock) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].host_sock == host_sock) {
            Room* room = &ctx->rooms[i];
            room->state = QUIZ_STATE_STARTED;
            
            LOG_INFO("Starting quiz for room");
            
            // Notify all participants (not the host) that quiz is available
            for (int j = 0; j < room->client_count; j++) {
                if (room->clients[j]) {
                    char start_msg[64];
                    snprintf(start_msg, sizeof(start_msg), "QUIZ_AVAILABLE:%d", room->quiz_duration);
                    net_send_to_client(room->clients[j]->sock, start_msg, strlen(start_msg));
                }
            }
            
            // Confirm to host
            char response[32];
            snprintf(response, sizeof(response), "QUIZ_STARTED:%d", room->client_count);
            net_send_to_client(host_sock, response, strlen(response));

                // Log start
                char log_entry[256];
                time_t now = time(NULL);
                struct tm* tm_info = localtime(&now);
                char ts[64];
                strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
                snprintf(log_entry, sizeof(log_entry), "[%s] QUIZ_START room_id=%d host=%s clients=%d duration=%d", 
                         ts, room->id, room->host_username, room->client_count, room->quiz_duration);
                storage_save_log(log_entry);
            
            return 0;
        }
    }
    net_send_to_client(host_sock, "ERROR:Not a room host", 21);
    return -1;
}

int room_client_start_quiz(ServerContext* ctx, int client_sock) {
    Room* room = find_room_by_client(ctx, client_sock);
    if (!room) {
        net_send_to_client(client_sock, "ERROR:Not in any room", 21);
        return -1;
    }
    
    if (room->state != QUIZ_STATE_STARTED) {
        net_send_to_client(client_sock, "ERROR:Quiz not available yet", 28);
        return -1;
    }
    
    // Check start_time and end_time validity
    time_t now_t = time(NULL);
    if (room->start_time > 0 && now_t < room->start_time) {
        char err[64];
        snprintf(err, sizeof(err), "ERROR:Quiz starts at %ld", room->start_time);
        net_send_to_client(client_sock, err, strlen(err));
        return -1;
    }
    
    if (room->end_time > 0 && now_t > room->end_time) {
        net_send_to_client(client_sock, "ERROR:Quiz has ended", 20);
        return -1;
    }
    
    Client* client = find_client_in_room(room, client_sock);
    if (!client) {
        net_send_to_client(client_sock, "ERROR:Client not found", 22);
        return -1;
    }
    
    // Start timer for this client
    client->quiz_start_time = time(NULL);
    client->is_taking_quiz = true;
    client->has_submitted = false;
    
    // Send questions to this client
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "QUESTIONS:%d;%s", room->quiz_duration, room->questions);
    net_send_to_client(client_sock, response, strlen(response));
    
    // If client has saved answers (reconnect scenario), send them
    if (strlen(client->answers) > 0) {
        char ans_msg[128];
        snprintf(ans_msg, sizeof(ans_msg), "ANSWERS:%s,%d", client->answers, client->current_question);
        net_send_to_client(client_sock, ans_msg, strlen(ans_msg));
    }
    
    // Notify host
    char notify[128];
    snprintf(notify, sizeof(notify), "PARTICIPANT_STARTED:%s", client->username);
    net_send_to_client(room->host_sock, notify, strlen(notify));
    
    LOG_INFO("Client started quiz");

        // Log participant start
        char log_entry[256];
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
        snprintf(log_entry, sizeof(log_entry), "[%s] QUIZ_TAKE_START room_id=%d user=%s", 
                 ts, room->id, client->username);
        storage_save_log(log_entry);
    return 0;
}

int room_get_stats(ServerContext* ctx, int host_sock) {
    // Find room where this client is the host
    Room* room = NULL;
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].host_sock == host_sock) {
            room = &ctx->rooms[i];
            break;
        }
    }
    
    if (!room) {
        net_send_to_client(host_sock, "ERROR:Not a room host", 21);
        return -1;
    }
            
            // Build stats response
            char response[BUFFER_SIZE];
            time_t now = time(NULL);
            
            int taking = 0;
            int submitted = 0;
            int waiting = 0;
            int submitted_count = 0;
            int sum_percent = 0;
            int best_percent = 0;
            int last_percent = 0;
            
            char participants[BUFFER_SIZE] = "";
            
            for (int j = 0; j < room->client_count; j++) {
                if (room->clients[j]) {
                    Client* c = room->clients[j];
                    
                    if (c->has_submitted) {
                        submitted++;
                        submitted_count++;
                        // Add submitted participant info
                        char info[128];
                        snprintf(info, sizeof(info), "%s%s:S:%d/%d", 
                            strlen(participants) > 0 ? "," : "",
                            c->username, c->score, c->total);
                        strcat(participants, info);

                        int percent = (c->total > 0) ? (c->score * 100 / c->total) : 0;
                        sum_percent += percent;
                        if (percent > best_percent) best_percent = percent;
                        last_percent = percent;
                    } else if (c->is_taking_quiz) {
                        taking++;
                        // Add taking participant info with remaining time
                        int elapsed = (int)(now - c->quiz_start_time);
                        int remaining = room->quiz_duration > 0 ? room->quiz_duration - elapsed : -1;
                        if (remaining < 0 && room->quiz_duration > 0) remaining = 0;
                        
                        char info[128];
                        snprintf(info, sizeof(info), "%s%s:T:%d", 
                            strlen(participants) > 0 ? "," : "",
                            c->username, remaining);
                        strcat(participants, info);
                    } else {
                        waiting++;
                        // Add waiting participant info
                        char info[128];
                        snprintf(info, sizeof(info), "%s%s:W:0", 
                            strlen(participants) > 0 ? "," : "",
                            c->username);
                        strcat(participants, info);
                    }
                }
            }
            
            int avg_percent = submitted_count > 0 ? sum_percent / submitted_count : 0;
            snprintf(response, sizeof(response), "ROOM_STATS:%d,%d,%d;%s;avg=%d,best=%d,last=%d", 
                     waiting, taking, submitted, participants, avg_percent, best_percent, last_percent);
            net_send_to_client(host_sock, response, strlen(response));
            return 0;
    
    return -1;
}

int room_submit_answers(ServerContext* ctx, int client_sock, const char* answers) {
    Room* room = find_room_by_client(ctx, client_sock);
    if (!room) {
        net_send_to_client(client_sock, "ERROR:Not in any room", 21);
        return -1;
    }
    
    Client* client = find_client_in_room(room, client_sock);
    if (!client) {
        net_send_to_client(client_sock, "ERROR:Client not found", 22);
        return -1;
    }
    
    // Calculate score by comparing each answer
    int score = 0;
    int total = 0;

    const char* correct = room->correct_answers;
    const char* submitted = answers;

    // Count total questions from correct answers (comma-separated)
    for (const char* p = correct; *p; p++) {
        if (*p == ',' || *(p + 1) == '\0') total++;
    }
    if (total == 0) total = 1;

    // Compare answers
    int idx = 0;
    char correct_copy[256];
    strncpy(correct_copy, correct, sizeof(correct_copy) - 1);
    correct_copy[sizeof(correct_copy) - 1] = '\0';

    char* token = strtok(correct_copy, ",");
    while (token && idx < (int)strlen(submitted)) {
        if (submitted[idx] == token[0]) {
            score++;
        }
        idx++;
        token = strtok(NULL, ",");
    }

    // Mark client as submitted
    client->has_submitted = true;
    client->is_taking_quiz = false;
    client->score = score;
    client->total = total;
    
    // Calculate time taken
    time_t now = time(NULL);
    int time_taken = client->quiz_start_time > 0 ? (int)(now - client->quiz_start_time) : 0;

    char result[64];
    snprintf(result, sizeof(result), "RESULT:%d/%d,%d", score, total, time_taken);
    net_send_to_client(client_sock, result, strlen(result));
    
    // Notify host
    char notify[128];
    snprintf(notify, sizeof(notify), "PARTICIPANT_SUBMITTED:%s,%d,%d", client->username, score, total);
    net_send_to_client(room->host_sock, notify, strlen(notify));
    
    LOG_INFO("Answers submitted and scored");
    
    // Log to file
    char log_entry[256];
    time_t now_t = time(NULL);
    struct tm* tm_info = localtime(&now_t);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(log_entry, sizeof(log_entry), "[%s] SUBMIT user=%s room_id=%d score=%d/%d time=%ds", 
             ts, client->username, room->id, score, total, time_taken);
    storage_save_log(log_entry);
    storage_save_result(room->id, room->host_username, client->username, score, total, time_taken);
    return 0;
}

int room_delete(ServerContext* ctx, int room_id) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].id == room_id) {
            Room* room = &ctx->rooms[i];
            
            // Notify all participants that room is closing
            for (int j = 0; j < room->client_count; j++) {
                if (room->clients[j]) {
                    net_send_to_client(room->clients[j]->sock, "ERROR:Room has been deleted", 27);
                }
            }
            
            // Shift remaining rooms down
            for (int j = i; j < ctx->room_count - 1; j++) {
                ctx->rooms[j] = ctx->rooms[j + 1];
            }
            ctx->room_count--;
            
            LOG_INFO("Room deleted");

                // Log room deletion
                char log_entry[256];
                time_t now = time(NULL);
                struct tm* tm_info = localtime(&now);
                char ts[64];
                strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
                snprintf(log_entry, sizeof(log_entry), "[%s] ROOM_DELETED id=%d", ts, room_id);
                storage_save_log(log_entry);
            return 0;
        }
    }
    return -1;
}