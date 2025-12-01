#include "client.h"
#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>

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
    memset(ctx->answers, 0, sizeof(ctx->answers));
    strcpy(ctx->status_message, "");
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
    } else {
        snprintf(buffer, BUFFER_SIZE, "%s:", type);
    }
    
    return net_send(ctx, buffer, strlen(buffer));
}

// Parse questions received from server
// New format: "Q1?A.opt|B.opt|C.opt|D.opt;Q2?A.opt|B.opt|C.opt|D.opt;..."
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
    
    while (question_token && ctx->question_count < MAX_QUESTIONS) {
        Question* q = &ctx->questions[ctx->question_count];
        q->id = ctx->question_count + 1;
        
        // Split question and options by '?'
        char* question_mark = strchr(question_token, '?');
        if (question_mark) {
            // Copy question text (before '?')
            size_t q_len = question_mark - question_token;
            if (q_len >= sizeof(q->question)) q_len = sizeof(q->question) - 1;
            strncpy(q->question, question_token, q_len);
            q->question[q_len] = '\0';
            
            // Parse options after '?'
            char* options_str = question_mark + 1;
            char options_copy[512];
            strncpy(options_copy, options_str, sizeof(options_copy) - 1);
            options_copy[sizeof(options_copy) - 1] = '\0';
            
            // Split options by '|'
            char* saveptr2;
            char* opt_token = strtok_r(options_copy, "|", &saveptr2);
            int opt_idx = 0;
            while (opt_token && opt_idx < 4) {
                strncpy(q->options[opt_idx], opt_token, sizeof(q->options[opt_idx]) - 1);
                opt_idx++;
                opt_token = strtok_r(NULL, "|", &saveptr2);
            }
        } else {
            // No options embedded, just question text
            strncpy(q->question, question_token, sizeof(q->question) - 1);
            strcpy(q->options[0], "A");
            strcpy(q->options[1], "B");
            strcpy(q->options[2], "C");
            strcpy(q->options[3], "D");
        }
        
        ctx->question_count++;
        question_token = strtok_r(NULL, ";", &saveptr1);
    }
    
    ctx->total_questions = ctx->question_count;
    ctx->current_question = 0;
}

// Process a message received from the server
void client_process_server_message(ClientContext* ctx, const char* message) {
    char type[32] = {0};
    char data[BUFFER_SIZE] = {0};
    
    // Parse message format "TYPE:DATA"
    const char* colon = strchr(message, ':');
    if (colon) {
        size_t type_len = colon - message;
        if (type_len > 31) type_len = 31;
        strncpy(type, message, type_len);
        strncpy(data, colon + 1, BUFFER_SIZE - 1);
    } else {
        strncpy(type, message, 31);
    }
    
    if (strcmp(type, "ROOM_CREATED") == 0) {
        ctx->current_room_id = atoi(data);
        ctx->is_host = true;
        snprintf(ctx->status_message, sizeof(ctx->status_message), 
                 "Room %d created! You are the host.", ctx->current_room_id);
        ctx->current_state = PAGE_QUIZ;
    } else if (strcmp(type, "JOINED") == 0) {
        // Parse room ID if included
        if (strlen(data) > 0) {
            ctx->current_room_id = atoi(data);
        }
        ctx->is_host = false;
        snprintf(ctx->status_message, sizeof(ctx->status_message), 
                 "Joined room %d! Waiting for host to start...", ctx->current_room_id);
        ctx->current_state = PAGE_QUIZ;
    } else if (strcmp(type, "RESULT") == 0) {
        // Parse result format "SCORE/TOTAL"
        sscanf(data, "%d/%d", &ctx->score, &ctx->total_questions);
        ctx->current_state = PAGE_RESULT;
    } else if (strcmp(type, "ERROR") == 0) {
        snprintf(ctx->status_message, sizeof(ctx->status_message), "Error: %.100s", data);
    } else if (strstr(message, "?") != NULL && strstr(message, ";") != NULL) {
        // This looks like questions data with new format (contains ? and ;)
        parse_questions(ctx, message);
        ctx->current_state = PAGE_QUIZ;
        snprintf(ctx->status_message, sizeof(ctx->status_message), 
                 "Quiz started! %d questions loaded.", ctx->question_count);
    } else if (strchr(message, ';') != NULL) {
        // Old format - semicolon separated questions
        parse_questions(ctx, message);
        if (ctx->question_count > 0) {
            ctx->current_state = PAGE_QUIZ;
            snprintf(ctx->status_message, sizeof(ctx->status_message), 
                     "Quiz started! %d questions loaded.", ctx->question_count);
        }
    } else {
        // Try to parse as questions if it contains semicolons
        if (strchr(message, ';')) {
            parse_questions(ctx, message);
            if (ctx->question_count > 0) {
                ctx->current_state = PAGE_QUIZ;
                snprintf(ctx->status_message, sizeof(ctx->status_message), 
                         "Quiz started! %d questions loaded.", ctx->question_count);
            }
        }
    }
}

// Receive and process any pending messages from server
int client_receive_message(ClientContext* ctx) {
    if (!ctx || !ctx->connected) return -1;
    
    char buffer[BUFFER_SIZE] = {0};
    int result = net_receive_nonblocking(ctx, buffer, BUFFER_SIZE, 10);
    
    if (result > 0) {
        client_process_server_message(ctx, buffer);
        return result;
    } else if (result == -2) {
        // Connection closed
        strcpy(ctx->status_message, "Disconnected from server!");
        ctx->connected = false;
        return -1;
    }
    
    return 0;
}

void client_run(ClientContext* ctx) {
    // Simple event loop
    // Uses non-blocking receive to check for server messages while handling UI input
    
    ctx->current_state = PAGE_LOGIN;

    while (ctx->running) {
        // Check for server messages if connected
        if (ctx->connected) {
            client_receive_message(ctx);
        }
        
        // Clear screen at start of frame
        erase();

        // Dispatch draw based on state
        switch (ctx->current_state) {
            case PAGE_LOGIN:
                page_login_draw(ctx);
                break;
            case PAGE_DASHBOARD:
                page_dashboard_draw(ctx);
                break;
            case PAGE_ROOM_LIST:
                page_room_list_draw(ctx);
                break;
            case PAGE_QUIZ:
                page_quiz_draw(ctx);
                break;
            case PAGE_RESULT:
                page_result_draw(ctx);
                break;
        }

        refresh();

        // Get input with timeout so we can check for server messages
        timeout(100); // 100ms timeout for getch
        int ch = ui_get_input();
        
        if (ch == ERR) {
            // No input, continue to check server messages
            continue;
        }

        // Global keys
        if (ch == KEY_F(10)) {
            ctx->running = false;
            continue;
        }

        // Dispatch input based on state
        switch (ctx->current_state) {
            case PAGE_LOGIN:
                page_login_handle_input(ctx, ch);
                break;
            case PAGE_DASHBOARD:
                page_dashboard_handle_input(ctx, ch);
                break;
            case PAGE_ROOM_LIST:
                page_room_list_handle_input(ctx, ch);
                break;
            case PAGE_QUIZ:
                page_quiz_handle_input(ctx, ch);
                break;
            case PAGE_RESULT:
                page_result_handle_input(ctx, ch);
                break;
        }
    }
}
