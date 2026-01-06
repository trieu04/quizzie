#include "client.h"
#include "ui.h"
#include "net.h"
#include <string.h>
#include <time.h>

ClientContext* client_init() {
    ClientContext* ctx = (ClientContext*)malloc(sizeof(ClientContext));
    if (!ctx) {
        perror("malloc");
        return NULL;
    }
    memset(ctx, 0, sizeof(ClientContext));
    ctx->socket_fd = -1;
    ctx->running = true;
    ctx->connected = false;
    ctx->current_room_id = -1;
    ctx->is_host = false;
    ctx->question_count = 0;
    ctx->current_question = 0;
    ctx->score = 0;
    ctx->total_questions = 0;
    ctx->quiz_duration = 300;  // Default 5 minutes
    ctx->quiz_available = false;
    ctx->quiz_start_time = 0;
    ctx->room_state = QUIZ_STATE_WAITING;
    ctx->participant_count = 0;
    ctx->last_room_id = -1;
    ctx->stats_avg_percent = 0;
    ctx->stats_best_percent = 0;
    ctx->stats_last_percent = 0;
    strncpy(ctx->question_file, "questions.txt", sizeof(ctx->question_file) - 1);
    ctx->question_file[sizeof(ctx->question_file) - 1] = '\0';
    memset(ctx->answers, 0, sizeof(ctx->answers));
    ctx->status_message[0] = '\0';  // Already memset, this is redundant but explicit
    // Initialize server connection with defaults
    strncpy(ctx->server_ip, "127.0.0.1", sizeof(ctx->server_ip) - 1);
    ctx->server_ip[sizeof(ctx->server_ip) - 1] = '\0';
    ctx->server_port = 8080;
    return ctx;
}

void client_cleanup(ClientContext* ctx) {
    if (ctx) {
        if (ctx->socket_fd != -1) {
            net_close(ctx);
        }
        free(ctx);
    }
}

// Send a message to the server in the format "TYPE:DATA"
int client_send_message(ClientContext* ctx, const char* type, const char* data) {
    if (!ctx || !ctx->connected) return -1;

    char buffer[BUFFER_SIZE];
    if (data && strlen(data) > 0) {
        snprintf(buffer, BUFFER_SIZE, "%s:%s", type, data);
    }
    else {
        snprintf(buffer, BUFFER_SIZE, "%s:", type);
    }

    return net_send(ctx, buffer, strlen(buffer));
}

// Parse questions received from server
// Format "duration;Q1?A.opt|B.opt|C.opt|D.opt;Q2?A.opt|B.opt|C.opt|D.opt;..."
static void parse_questions(ClientContext* ctx, const char* data) {
    ctx->question_count = 0;
    memset(ctx->questions, 0, sizeof(ctx->questions));
    memset(ctx->answers, 0, sizeof(ctx->answers));

    char buffer[BUFFER_SIZE];
    strncpy(buffer, data, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';

    // Split by semicolon for each question
    char* saveptr1;
    char* question_token = strtok_r(buffer, ";", &saveptr1);

    // Check if first token is a duration (number only)
    if (question_token) {
        bool is_duration = true;
        for (int i = 0; question_token[i]; i++) {
            if (question_token[i] < '0' || question_token[i] > '9') {
                is_duration = false;
                break;
            }
        }
        if (is_duration && strlen(question_token) < 6) {
            ctx->quiz_duration = atoi(question_token);
            question_token = strtok_r(NULL, ";", &saveptr1);
        }
    }

    while (question_token && ctx->question_count < MAX_QUESTIONS) {
        Question* q = &ctx->questions[ctx->question_count];
        q->id = ctx->question_count + 1;
        strncpy(q->difficulty, "medium", sizeof(q->difficulty) - 1);
        q->difficulty[sizeof(q->difficulty) - 1] = '\0';

        // Split question and options by '?'
        char* question_mark = strchr(question_token, '?');
        if (question_mark) {
            // Copy question text (before '?')
            size_t q_len = question_mark - question_token;
            if (q_len >= sizeof(q->question)) q_len = sizeof(q->question) - 1;
            strncpy(q->question, question_token, q_len);
            q->question[q_len] = '\0';

            // Parse options after '?'
            char* options_str = question_mark + 2;
            char options_copy[512];
            strncpy(options_copy, options_str, sizeof(options_copy) - 1);
            options_copy[sizeof(options_copy) - 1] = '\0';

            // Extract optional difficulty suffix after '^'
            char* diff_sep = strrchr(options_copy, '^');
            if (diff_sep && *(diff_sep + 1)) {
                *diff_sep = '\0';
                strncpy(q->difficulty, diff_sep + 1, sizeof(q->difficulty) - 1);
                q->difficulty[sizeof(q->difficulty) - 1] = '\0';
            }

            // Split options by '|'
            char* saveptr2;
            char* opt_token = strtok_r(options_copy, "|", &saveptr2);
            int opt_idx = 0;
            while (opt_token && opt_idx < 4) {
                // Trim ?A., ?B., etc. if present
                if (strlen(opt_token) > 2 && opt_token[1] == '.') {
                    strncpy(q->options[opt_idx], opt_token + 2, sizeof(q->options[opt_idx]) - 1);
                } else {
                    strncpy(q->options[opt_idx], opt_token, sizeof(q->options[opt_idx]) - 1);
                }
                opt_idx++;
                opt_token = strtok_r(NULL, "|", &saveptr2);
            }
        }
        else {
            // No options embedded, just question text
            strncpy(q->question, question_token, sizeof(q->question) - 1);
            q->question[sizeof(q->question) - 1] = '\0';
            strncpy(q->options[0], "A", sizeof(q->options[0]) - 1);
            strncpy(q->options[1], "B", sizeof(q->options[1]) - 1);
            strncpy(q->options[2], "C", sizeof(q->options[2]) - 1);
            strncpy(q->options[3], "D", sizeof(q->options[3]) - 1);
        }

        ctx->question_count++;
        question_token = strtok_r(NULL, ";", &saveptr1);
    }

    ctx->total_questions = ctx->question_count;
    ctx->current_question = 0;
}

// Parse room list from server
// Format: "room_id,host_username,player_count,state;room_id2,..."
static void parse_room_list(ClientContext* ctx, const char* data) {
    ctx->room_count = 0;
    
    if (strlen(data) == 0) {
        return;
    }
    
    char buffer[BUFFER_SIZE];
    strncpy(buffer, data, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    char* saveptr;
    char* token = strtok_r(buffer, ";", &saveptr);
    
    while (token && ctx->room_count < MAX_ROOMS_DISPLAY) {
        RoomInfo* r = &ctx->rooms[ctx->room_count];
        memset(r, 0, sizeof(RoomInfo));
        
        // Parse: room_id,host_username,player_count,state
        char host[32] = "";
        if (sscanf(token, "%d,%31[^,],%d,%d", &r->id, host, &r->player_count, &r->state) >= 2) {
            strncpy(r->host_username, host, sizeof(r->host_username) - 1);
            // Check if this is user's room
            r->is_my_room = (strcmp(r->host_username, ctx->username) == 0);
            ctx->room_count++;
        }
        token = strtok_r(NULL, ";", &saveptr);
    }
}

// Parse room stats from server
static void parse_room_stats(ClientContext* ctx, const char* data) {
    // Format: "waiting,taking,submitted;user1:status:value,user2:status:value,..."
    ctx->participant_count = 0;
    ctx->stats_waiting = 0;
    ctx->stats_taking = 0;
    ctx->stats_submitted = 0;
    ctx->stats_avg_percent = 0;
    ctx->stats_best_percent = 0;
    ctx->stats_last_percent = 0;
    
    char buffer[BUFFER_SIZE];
    strncpy(buffer, data, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    // Parse summary (find first semicolon)
    char* participants = NULL;
    char* agg_section = NULL;
    char* first = strchr(buffer, ';');
    if (first) {
        *first = '\0';
        sscanf(buffer, "%d,%d,%d", &ctx->stats_waiting, &ctx->stats_taking, &ctx->stats_submitted);

        participants = first + 1;
        char* second = strchr(participants, ';');
        if (second) {
            *second = '\0';
            agg_section = second + 1;
        }

        if (strlen(participants) > 0) {
            char* saveptr;
            char* token = strtok_r(participants, ",", &saveptr);
            while (token && ctx->participant_count < MAX_PARTICIPANTS) {
                ParticipantInfo* p = &ctx->participants[ctx->participant_count];
                char status;
                int value = 0;
                
                if (sscanf(token, "%[^:]:%c:%d", p->username, &status, &value) >= 2) {
                    p->status = status;
                    if (status == 'T') {
                        p->remaining_time = value;
                    } else if (status == 'S') {
                        // Parse score/total
                        char* slash = strchr(token, '/');
                        if (slash) {
                            sscanf(strchr(token, ':') + 3, "%d/%d", &p->score, &p->total);
                        } else {
                            p->score = value;
                            p->total = 0;
                        }
                    }
                    ctx->participant_count++;
                }
                token = strtok_r(NULL, ",", &saveptr);
            }
        }

        if (agg_section && strlen(agg_section) > 0) {
            char agg_copy[256];
            strncpy(agg_copy, agg_section, sizeof(agg_copy) - 1);
            agg_copy[sizeof(agg_copy) - 1] = '\0';
            char* saveptr2;
            char* token = strtok_r(agg_copy, ",", &saveptr2);
            while (token) {
                char key[16] = {0};
                int val = 0;
                if (sscanf(token, "%15[^=]=%d", key, &val) == 2) {
                    if (strcmp(key, "avg") == 0) ctx->stats_avg_percent = val;
                    else if (strcmp(key, "best") == 0) ctx->stats_best_percent = val;
                    else if (strcmp(key, "last") == 0) ctx->stats_last_percent = val;
                }
                token = strtok_r(NULL, ",", &saveptr2);
            }
        }
    }
}

// Helper to safely set status messages with bounded server-provided detail
static void set_status_with_data(ClientContext* ctx, const char* prefix, const char* data) {
    if (!ctx) return;
    const char* d = data ? data : "";
    size_t prefix_len = strlen(prefix);
    size_t max_detail = sizeof(ctx->status_message) - 1;
    if (prefix_len < max_detail) {
        max_detail -= prefix_len;
    } else {
        max_detail = 0;
    }
    snprintf(ctx->status_message, sizeof(ctx->status_message), "%s%.*s", prefix, (int)max_detail, d);
}

// Process a message received from the server
void client_process_server_message(ClientContext* ctx, const char* message) {
    char type[32] = { 0 };
    char data[BUFFER_SIZE] = { 0 };

    // Parse message format "TYPE:DATA"
    const char* colon = strchr(message, ':');
    if (colon) {
        size_t type_len = colon - message;
        if (type_len > 31) type_len = 31;
        strncpy(type, message, type_len);
        strncpy(data, colon + 1, BUFFER_SIZE - 1);
    }
    else {
        strncpy(type, message, 31);
    }

    if (strcmp(type, "ROOM_CREATED") == 0) {
        // Format: room_id,duration,total_questions
        int room_id = 0, duration = 300, total_q = 0;
        
        // Try parsing with total_questions first
        if (sscanf(data, "%d,%d,%d", &room_id, &duration, &total_q) >= 2) {
            ctx->current_room_id = room_id;
            ctx->quiz_duration = duration;
            if (strchr(data, ',') && strchr(strchr(data, ',')+1, ',')) {
                 ctx->total_questions = total_q;
            } else {
                 ctx->total_questions = 0; // Legacy
            }
        }
        ctx->is_host = true;
        ctx->room_state = QUIZ_STATE_STARTED; // Auto-started (Time based)
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Room %d created! You are the host.", ctx->current_room_id);
        ctx->current_state = PAGE_HOST_PANEL;
    }
    else if (strcmp(type, "ROOM_REJOINED") == 0) {
        // Format: room_id,duration,state
        int room_id = 0, duration = 300, state = 0;
        if (sscanf(data, "%d,%d,%d", &room_id, &duration, &state) >= 1) {
            ctx->current_room_id = room_id;
            ctx->quiz_duration = duration;
            ctx->room_state = (QuizState)state;
        }
        ctx->is_host = true;
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Rejoined Room %d as host.", ctx->current_room_id);
        ctx->current_state = PAGE_HOST_PANEL;
    }
    else if (strcmp(type, "JOINED") == 0) {
        // Format: room_id,duration,state
        int room_id = 0, duration = 300, state = 0;
        if (sscanf(data, "%d,%d,%d", &room_id, &duration, &state) >= 1) {
            ctx->current_room_id = room_id;
            ctx->last_room_id = room_id;  // Save for reconnect
            ctx->quiz_duration = duration;
            ctx->room_state = (QuizState)state;
            ctx->quiz_available = (state == QUIZ_STATE_STARTED);
        }
        ctx->is_host = false;
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Joined room %d! %s", ctx->current_room_id,
            ctx->quiz_available ? "Click 'Start Quiz' to begin!" : "Waiting for host to start...");
        ctx->current_state = PAGE_QUIZ;
    }
    else if (strcmp(type, "QUIZ_AVAILABLE") == 0) {
        // Host has started the quiz, client can now begin
        ctx->quiz_available = true;
        if (strlen(data) > 0) {
            ctx->quiz_duration = atoi(data);
        }
        ctx->room_state = QUIZ_STATE_STARTED;
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Quiz is ready! Click 'Start Quiz' to begin (%d seconds)", ctx->quiz_duration);
        // Navigate to quiz page to show start button (force refresh)
        ctx->current_state = PAGE_QUIZ;
        ctx->force_page_refresh = true;
    }
    else if (strcmp(type, "QUIZ_STARTED") == 0) {
        // Confirmation for host that quiz has started
        ctx->room_state = QUIZ_STATE_STARTED;
        int participant_count = atoi(data);
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Quiz started! %d participants", participant_count);
    }
    else if (strcmp(type, "QUESTIONS") == 0) {
        // Client receives questions after pressing start
        parse_questions(ctx, data);
        ctx->quiz_start_time = time(NULL);
        // Force page navigation to refresh quiz UI with questions
        ctx->current_state = PAGE_QUIZ;
        ctx->force_page_refresh = true;
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Quiz started! %d questions, %d seconds", ctx->question_count, ctx->quiz_duration);
    }
    else if (strcmp(type, "ROOM_STATS") == 0) {
        parse_room_stats(ctx, data);
    }
    else if (strcmp(type, "ROOM_LIST") == 0) {
        parse_room_list(ctx, data);
        ctx->status_message[0] = '\0';
    }
    else if (strcmp(type, "CONFIG_UPDATED") == 0) {
        // Format: duration,filename
        char data_copy[256];
        strncpy(data_copy, data, sizeof(data_copy) - 1);
        data_copy[sizeof(data_copy) - 1] = '\0';
        
        char filename[64] = "";
        int duration = 300;
        char* comma = strchr(data_copy, ',');
        if (comma) {
            *comma = '\0';
            duration = atoi(data_copy);
            strncpy(filename, comma + 1, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        } else {
            duration = atoi(data_copy);
        }
        ctx->quiz_duration = duration;
        if (strlen(filename) > 0) {
            strncpy(ctx->question_file, filename, sizeof(ctx->question_file) - 1);
            ctx->question_file[sizeof(ctx->question_file) - 1] = '\0';
        }
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Config updated: %ds, %.90s", ctx->quiz_duration, ctx->question_file);
    }
    else if (strcmp(type, "PARTICIPANT_JOINED") == 0) {
        set_status_with_data(ctx, "New participant: ", data);
    }
    else if (strcmp(type, "PARTICIPANT_STARTED") == 0) {
        const char* d = data;
        size_t suffix_len = strlen(" started the quiz");
        size_t max_name = sizeof(ctx->status_message) - 1;
        if (suffix_len < max_name) {
            max_name -= suffix_len;
        } else {
            max_name = 0;
        }
        snprintf(ctx->status_message, sizeof(ctx->status_message), "%.*s started the quiz", (int)max_name, d);
    }
    else if (strcmp(type, "PARTICIPANT_SUBMITTED") == 0) {
        // Format: username,score,total (we still bound display to fit)
        set_status_with_data(ctx, "Submitted: ", data);
    }
    else if (strcmp(type, "RESULT") == 0) {
        // Format: SCORE/TOTAL,time_taken
        char data_copy[256];
        strncpy(data_copy, data, sizeof(data_copy) - 1);
        data_copy[sizeof(data_copy) - 1] = '\0';
        
        int time_taken = 0;
        char* comma = strchr(data_copy, ',');
        if (comma) {
            *comma = '\0';
            time_taken = atoi(comma + 1);
        }
        sscanf(data_copy, "%d/%d", &ctx->score, &ctx->total_questions);
        ctx->time_taken = time_taken;
        ctx->current_state = PAGE_RESULT;
    }
    else if (strcmp(type, "LOGIN_SUCCESS") == 0) {
        // Parse role from data (0=participant, 1=admin)
        ctx->role = atoi(data);
        ctx->force_page_refresh = false;  // Clear any previous force refresh flag
        if (ctx->role == 1) {
            ctx->current_state = PAGE_ADMIN_PANEL;
            snprintf(ctx->status_message, sizeof(ctx->status_message), "Admin logged in!");
        } else {
            ctx->current_state = PAGE_DASHBOARD;
            snprintf(ctx->status_message, sizeof(ctx->status_message), "Logged in successfully!");
        }
    }
    else if (strcmp(type, "LOGIN_FAILED") == 0) {
        snprintf(ctx->status_message, sizeof(ctx->status_message), "%.120s", data);
        // Disconnect on login failure to prevent showing 'connected' status
        net_close(ctx);
        ctx->connected = false;
        ctx->current_state = PAGE_LOGIN;
        ctx->force_page_refresh = true;  // Force refresh to show error message
    }
    else if (strcmp(type, "FORCE_LOGOUT") == 0) {
        // Another user logged in with same account - force logout
        snprintf(ctx->status_message, sizeof(ctx->status_message), "%.120s", data);
        net_close(ctx);
        ctx->connected = false;
        ctx->current_state = PAGE_LOGIN;
        ctx->force_page_refresh = true;  // Force refresh to show login page with message
    }
    else if (strcmp(type, "REGISTER_SUCCESS") == 0) {
        ctx->force_page_refresh = false;  // Clear any previous force refresh flag
        snprintf(ctx->status_message, sizeof(ctx->status_message), "Registration successful! You can now login.");
    }
    else if (strcmp(type, "REGISTER_FAILED") == 0) {
        snprintf(ctx->status_message, sizeof(ctx->status_message), "%.120s", data);
        // Disconnect on registration failure to prevent showing 'connected' status
        net_close(ctx);
        ctx->connected = false;
        ctx->current_state = PAGE_REGISTER;
        ctx->force_page_refresh = true;  // Force refresh to show error message
    }
    else if (strcmp(type, "UPLOAD_SUCCESS") == 0) {
        snprintf(ctx->status_message, sizeof(ctx->status_message), "CSV uploaded successfully!");
    }
    else if (strcmp(type, "UPLOAD_FAILED") == 0) {
        snprintf(ctx->status_message, sizeof(ctx->status_message), "%.120s", data);
    }
    else if (strcmp(type, "ANSWERS") == 0) {
        // Format: ANSWERS:answer_string,current_question
        char data_copy[BUFFER_SIZE];
        strncpy(data_copy, data, sizeof(data_copy) - 1);
        data_copy[sizeof(data_copy) - 1] = '\0';
        
        char ans_str[MAX_QUESTIONS + 1] = {0};
        int cur_q = 0;
        char* comma = strchr(data_copy, ',');
        if (comma) {
            size_t len = comma - data_copy;
            if (len < sizeof(ans_str)) {
                strncpy(ans_str, data_copy, len);
                ans_str[len] = '\0';
                cur_q = atoi(comma + 1);
            }
        } else {
            strncpy(ans_str, data_copy, sizeof(ans_str) - 1);
        }
        
        // Restore answers
        for (int i = 0; i < (int)strlen(ans_str) && i < MAX_QUESTIONS; i++) {
            if (ans_str[i] != '-' && ans_str[i] != '\0') {
                ctx->answers[i] = ans_str[i];
            }
        }
        if (cur_q >= 0 && cur_q < ctx->question_count) {
            ctx->current_question = cur_q;
        }
        snprintf(ctx->status_message, sizeof(ctx->status_message), "Reconnected! Progress restored.");
    }
    else if (strcmp(type, "PRACTICE_QUESTIONS") == 0) {
        // Format: PRACTICE_QUESTIONS:questions_string,answers_string
        char data_copy[BUFFER_SIZE];
        strncpy(data_copy, data, sizeof(data_copy) - 1);
        data_copy[sizeof(data_copy) - 1] = '\0';
        
        char q_str[BUFFER_SIZE] = {0};
        char ans_str[MAX_QUESTIONS + 1] = {0};
        
        char* comma = strchr(data_copy, ',');
        if (comma) {
            size_t q_len = comma - data_copy;
            if (q_len < sizeof(q_str)) {
                strncpy(q_str, data_copy, q_len);
                q_str[q_len] = '\0';
            }
            strncpy(ans_str, comma + 1, sizeof(ans_str) - 1);
            ans_str[sizeof(ans_str) - 1] = '\0';
        }
        
        // Parse questions string (same format as QUESTIONS)
        if (strlen(q_str) > 0) {
            parse_questions(ctx, q_str);
            ctx->is_practice = true;
            ctx->current_room_id = -1;
            ctx->is_host = false;
            ctx->quiz_start_time = time(NULL);
            
            // Set practice answers for scoring
            if (strlen(ans_str) > 0) {
                strncpy(ctx->practice_answers, ans_str, sizeof(ctx->practice_answers) - 1);
            }
            
            ctx->current_state = PAGE_QUIZ;
            ctx->force_page_refresh = true;
            snprintf(ctx->status_message, sizeof(ctx->status_message),
                "Practice loaded! %d questions, %d seconds", ctx->question_count, ctx->quiz_duration);
        } else {
            strncpy(ctx->status_message, "Failed to load practice questions", sizeof(ctx->status_message) - 1);
        }
    }
    else if (strcmp(type, "REJOINED") == 0) {
        // Format: REJOINED:room_id,duration,state,remaining
        int room_id = 0, duration = 300, state = 0, remaining = 0;
        if (sscanf(data, "%d,%d,%d,%d", &room_id, &duration, &state, &remaining) >= 3) {
            ctx->current_room_id = room_id;
            ctx->quiz_duration = remaining > 0 ? remaining : duration;
            ctx->room_state = (QuizState)state;
            ctx->quiz_available = (state == QUIZ_STATE_STARTED);
        }
        ctx->is_host = false;
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Rejoined room %d! Time remaining: %d seconds", ctx->current_room_id, ctx->quiz_duration);
        ctx->current_state = PAGE_QUIZ;
        ctx->force_page_refresh = true;
    }
    else if (strcmp(type, "ERROR") == 0) {
        snprintf(ctx->status_message, sizeof(ctx->status_message), "Error: %.100s", data);
    }
    else if (strstr(message, "?") != NULL && strstr(message, ";") != NULL) {
        // This looks like questions data with new format (contains ? and ;)
        parse_questions(ctx, message);
        ctx->quiz_start_time = time(NULL);
        ctx->current_state = PAGE_QUIZ;
        snprintf(ctx->status_message, sizeof(ctx->status_message),
            "Quiz started! %d questions loaded.", ctx->question_count);
    }
    else if (strcmp(type, "QUESTION_FILES") == 0) {
        // Format: QUESTION_FILES:file1.csv|E,M,H,T;file2.csv|E,M,H,T...
        strncpy(ctx->available_files, data, sizeof(ctx->available_files) - 1);
        ctx->available_files[sizeof(ctx->available_files) - 1] = '\0';
        
        // Parse into list
        ctx->available_files_count = 0;
        char buf[4096];
        strncpy(buf, data, sizeof(buf)-1);
        buf[sizeof(buf)-1] = '\0';
        
        char* tokenizer = buf;
        char* token = strtok(tokenizer, ";");
        while (token && ctx->available_files_count < 50) {
            // token: "name|e,m,h,t"
            char* pipe = strchr(token, '|');
            if (pipe) {
                *pipe = '\0';
                char* stats = pipe + 1;
                // Parse Name
                strncpy(ctx->available_files_list[ctx->available_files_count].name, token, 127);
                ctx->available_files_list[ctx->available_files_count].name[127] = '\0';
                
                // Parse Stats
                int e=0, m=0, h=0, t=0;
                sscanf(stats, "%d,%d,%d,%d", &e, &m, &h, &t);
                ctx->available_files_list[ctx->available_files_count].easy_cnt = e;
                ctx->available_files_list[ctx->available_files_count].med_cnt = m;
                ctx->available_files_list[ctx->available_files_count].hard_cnt = h;
                ctx->available_files_list[ctx->available_files_count].total_cnt = t;
                
                ctx->available_files_count++;
            } else {
                // Legacy support (just name)
                 strncpy(ctx->available_files_list[ctx->available_files_count].name, token, 127);
                 ctx->available_files_list[ctx->available_files_count].name[127] = '\0';
                 ctx->available_files_list[ctx->available_files_count].total_cnt = 0; // Unknown
                 ctx->available_files_count++;
            }
            token = strtok(NULL, ";");
        }
        
        ctx->files_refreshed = true;
    }
    else if (strcmp(type, "PRACTICE_SUBJECTS") == 0) {
        // Format: PRACTICE_SUBJECTS:subject1|e,m,h;subject2|e,m,h...
        strncpy(ctx->practice_subjects, data, sizeof(ctx->practice_subjects) - 1);
        ctx->practice_subjects[sizeof(ctx->practice_subjects) - 1] = '\0';
        
        // Parse into list
        ctx->practice_subjects_count = 0;
        char buf[1024];
        strncpy(buf, data, sizeof(buf)-1);
        buf[sizeof(buf)-1] = '\0';
        
        // Split by semicolon for each subject
        char* subject_token = strtok(buf, ";");
        while (subject_token && ctx->practice_subjects_count < 50) {
            // Parse subject|easy,medium,hard
            char* pipe = strchr(subject_token, '|');
            if (pipe) {
                *pipe = '\0';
                char* stats = pipe + 1;
                
                // Copy subject name
                strncpy(ctx->practice_subjects_list[ctx->practice_subjects_count].name, 
                        subject_token, 63);
                ctx->practice_subjects_list[ctx->practice_subjects_count].name[63] = '\0';
                
                // Parse e,m,h
                int e=0, m=0, h=0;
                sscanf(stats, "%d,%d,%d", &e, &m, &h);
                ctx->practice_subjects_list[ctx->practice_subjects_count].easy_count = e;
                ctx->practice_subjects_list[ctx->practice_subjects_count].medium_count = m;
                ctx->practice_subjects_list[ctx->practice_subjects_count].hard_count = h;
                
                ctx->practice_subjects_count++;
            }
            subject_token = strtok(NULL, ";");
        }
        
        ctx->subjects_refreshed = true;
    }
}

// Receive and process any pending messages from server
int client_receive_message(ClientContext* ctx) {
    if (!ctx || !ctx->connected) return -1;

    char temp_buf[BUFFER_SIZE] = { 0 };
    int result = net_receive_nonblocking(ctx, temp_buf, BUFFER_SIZE - 1, 10);

    if (result > 0) {
        // Append to receive buffer
        if (ctx->recv_len + result < (int)sizeof(ctx->recv_buffer) - 1) {
            memcpy(ctx->recv_buffer + ctx->recv_len, temp_buf, result);
            ctx->recv_len += result;
            ctx->recv_buffer[ctx->recv_len] = '\0';
        } else {
            // Buffer overflow, reset
            ctx->recv_len = 0;
            ctx->recv_buffer[0] = '\0';
            return 0;
        }
        
        // Parse complete messages (delimited by \n)
        char* line_start = ctx->recv_buffer;
        char* newline;
        int processed = 0;
        while ((newline = strchr(line_start, '\n')) != NULL) {
            *newline = '\0';  // Terminate the line
            
            if (strlen(line_start) > 0) {
                client_process_server_message(ctx, line_start);
                processed++;
            }
            
            line_start = newline + 1;
        }
        
        // Move remaining incomplete data to start of buffer
        int remaining = strlen(line_start);
        if (remaining > 0) {
            memmove(ctx->recv_buffer, line_start, remaining + 1);
            ctx->recv_len = remaining;
        } else {
            ctx->recv_buffer[0] = '\0';
            ctx->recv_len = 0;
        }
        
        return processed > 0 ? result : 0;
    }
    else if (result == -2) {
        // Connection closed
        snprintf(ctx->status_message, sizeof(ctx->status_message), 
                 "Username or Password is Wrong!");
        ctx->connected = false;
        return -1;
    }

    return 0;
}

void client_run(ClientContext* ctx) {
    // GTK handles the main event loop
    // Navigation and updates are managed through UI callbacks and timers
    ctx->current_state = PAGE_LOGIN;
    ctx->running = true;
}
