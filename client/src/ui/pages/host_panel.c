#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static int selected_option = 0;
static int edit_mode = 0;  // 0: menu, 1: duration, 2: filename
static char duration_input[16] = "";
static char filename_input[64] = "";
static time_t last_stats_request = 0;

void page_host_panel_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Sidebar (Left 30%)
    int sidebar_width = col * 3 / 10;
    for (int i = 0; i < row - 1; ++i) {
        mvaddch(i, sidebar_width, ACS_VLINE);
    }

    // Sidebar content - Room Info
    attron(A_BOLD);
    mvprintw(1, 2, "HOST PANEL");
    attroff(A_BOLD);

    mvprintw(3, 2, "Room ID: %d", ctx->current_room_id);
    mvprintw(4, 2, "Host: %s", ctx->username);
    
    attron(A_BOLD);
    mvprintw(6, 2, "Status:");
    attroff(A_BOLD);
    
    const char* state_str = "Waiting";
    if (ctx->room_state == QUIZ_STATE_STARTED) state_str = "Quiz Active";
    else if (ctx->room_state == QUIZ_STATE_FINISHED) state_str = "Finished";
    mvprintw(7, 4, "%s", state_str);

    // Config section
    attron(A_BOLD);
    mvprintw(9, 2, "Configuration:");
    attroff(A_BOLD);
    
    if (edit_mode == 1) attron(A_REVERSE);
    mvprintw(10, 4, "Duration: %d sec", ctx->quiz_duration);
    if (edit_mode == 1) {
        mvprintw(10, 4, "Duration: %s_", duration_input);
    }
    if (edit_mode == 1) attroff(A_REVERSE);
    
    if (edit_mode == 2) attron(A_REVERSE);
    mvprintw(11, 4, "File: %.20s", ctx->question_file);
    if (edit_mode == 2) {
        mvprintw(11, 4, "File: %s_", filename_input);
    }
    if (edit_mode == 2) attroff(A_REVERSE);

    // Menu options
    int menu_y = 14;
    attron(A_BOLD);
    mvprintw(menu_y - 1, 2, "Actions:");
    attroff(A_BOLD);
    
    if (edit_mode == 0) {
        if (selected_option == 0) attron(A_REVERSE);
        if (ctx->room_state == QUIZ_STATE_WAITING) {
            mvprintw(menu_y, 4, "[S] Start Quiz");
        } else {
            mvprintw(menu_y, 4, "[R] Refresh Stats");
        }
        if (selected_option == 0) attroff(A_REVERSE);

        if (selected_option == 1) attron(A_REVERSE);
        mvprintw(menu_y + 1, 4, "[D] Set Duration");
        if (selected_option == 1) attroff(A_REVERSE);

        if (selected_option == 2) attron(A_REVERSE);
        mvprintw(menu_y + 2, 4, "[F] Set File");
        if (selected_option == 2) attroff(A_REVERSE);

        if (selected_option == 3) attron(A_REVERSE);
        mvprintw(menu_y + 3, 4, "[Q] Leave Room");
        if (selected_option == 3) attroff(A_REVERSE);
    }

    // Main Content (Right 70%)
    int main_x = sidebar_width + 3;
    int main_width = col - main_x - 2;

    // Status message at top
    if (strlen(ctx->status_message) > 0) {
        mvprintw(1, main_x, "%.60s", ctx->status_message);
    }

    // Statistics Header
    attron(A_BOLD);
    mvprintw(3, main_x, "PARTICIPANTS STATISTICS");
    attroff(A_BOLD);
    
    mvhline(4, main_x, ACS_HLINE, main_width);
    
    // Summary stats
    mvprintw(5, main_x, "Waiting: %d | Taking: %d | Submitted: %d | Total: %d",
        ctx->stats_waiting, ctx->stats_taking, ctx->stats_submitted,
        ctx->stats_waiting + ctx->stats_taking + ctx->stats_submitted);

    mvhline(6, main_x, ACS_HLINE, main_width);

    // Participants table header
    attron(A_BOLD);
    mvprintw(7, main_x, "%-20s %-12s %-15s", "Username", "Status", "Info");
    attroff(A_BOLD);
    
    mvhline(8, main_x, ACS_HLINE, main_width);

    // Participants list
    int y = 9;
    int max_display = row - 14;
    
    for (int i = 0; i < ctx->participant_count && i < max_display; i++) {
        ParticipantInfo* p = &ctx->participants[i];
        
        const char* status_str = "Waiting";
        char info_str[32] = "";
        
        if (p->status == 'T') {
            status_str = "Taking";
            int mins = p->remaining_time / 60;
            int secs = p->remaining_time % 60;
            sprintf(info_str, "%02d:%02d left", mins, secs);
            attron(COLOR_PAIR(2));  // Yellow/warning
        } else if (p->status == 'S') {
            status_str = "Submitted";
            sprintf(info_str, "%d/%d", p->score, p->total);
            attron(COLOR_PAIR(1));  // Green/success
        } else {
            attron(COLOR_PAIR(3));  // Default
        }
        
        mvprintw(y + i, main_x, "%-20s %-12s %-15s", p->username, status_str, info_str);
        attroff(COLOR_PAIR(1));
        attroff(COLOR_PAIR(2));
        attroff(COLOR_PAIR(3));
    }

    if (ctx->participant_count == 0) {
        mvprintw(y, main_x, "No participants yet. Share Room ID: %d", ctx->current_room_id);
    }

    // Footer
    if (edit_mode == 0) {
        mvprintw(row - 2, 2, "UP/DOWN: Select | ENTER: Action | ESC: Leave | F10: Quit");
    } else {
        mvprintw(row - 2, 2, "Type value then ENTER | ESC: Cancel");
    }
}

void page_host_panel_handle_input(ClientContext* ctx, int input) {
    // Request stats periodically
    time_t now = time(NULL);
    if (ctx->room_state == QUIZ_STATE_STARTED && now - last_stats_request >= 2) {
        if (ctx->connected) {
            client_send_message(ctx, "GET_STATS", "");
            last_stats_request = now;
        }
    }

    if (edit_mode == 1) {
        // Editing duration
        if (input == 27) {  // ESC
            edit_mode = 0;
            duration_input[0] = '\0';
        } else if (input == '\n' || input == KEY_ENTER) {
            if (strlen(duration_input) > 0) {
                ctx->quiz_duration = atoi(duration_input);
                // Send config update to server
                char config[128];
                sprintf(config, "%d,%s", ctx->quiz_duration, ctx->question_file);
                client_send_message(ctx, "SET_CONFIG", config);
            }
            edit_mode = 0;
            duration_input[0] = '\0';
        } else if (input == KEY_BACKSPACE || input == 127) {
            size_t len = strlen(duration_input);
            if (len > 0) duration_input[len - 1] = '\0';
        } else if (input >= '0' && input <= '9' && strlen(duration_input) < 5) {
            size_t len = strlen(duration_input);
            duration_input[len] = input;
            duration_input[len + 1] = '\0';
        }
        return;
    }
    
    if (edit_mode == 2) {
        // Editing filename
        if (input == 27) {  // ESC
            edit_mode = 0;
            filename_input[0] = '\0';
        } else if (input == '\n' || input == KEY_ENTER) {
            if (strlen(filename_input) > 0) {
                strncpy(ctx->question_file, filename_input, sizeof(ctx->question_file) - 1);
                // Send config update to server
                char config[128];
                sprintf(config, "%d,%s", ctx->quiz_duration, ctx->question_file);
                client_send_message(ctx, "SET_CONFIG", config);
            }
            edit_mode = 0;
            filename_input[0] = '\0';
        } else if (input == KEY_BACKSPACE || input == 127) {
            size_t len = strlen(filename_input);
            if (len > 0) filename_input[len - 1] = '\0';
        } else if ((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') ||
                   (input >= '0' && input <= '9') || input == '.' || input == '_' || input == '-') {
            if (strlen(filename_input) < 60) {
                size_t len = strlen(filename_input);
                filename_input[len] = input;
                filename_input[len + 1] = '\0';
            }
        }
        return;
    }

    // Normal menu mode
    if (input == KEY_UP) {
        selected_option = (selected_option - 1 + 4) % 4;
    } else if (input == KEY_DOWN) {
        selected_option = (selected_option + 1) % 4;
    } else if (input == 27) {  // ESC
        // Leave room
        ctx->current_room_id = -1;
        ctx->is_host = false;
        ctx->room_state = QUIZ_STATE_WAITING;
        ctx->participant_count = 0;
        ctx->current_state = PAGE_DASHBOARD;
    } else if (input == '\n' || input == KEY_ENTER) {
        if (selected_option == 0) {
            if (ctx->room_state == QUIZ_STATE_WAITING) {
                // Start quiz
                client_send_message(ctx, "START_GAME", "");
                strcpy(ctx->status_message, "Starting quiz...");
            } else {
                // Refresh stats
                client_send_message(ctx, "GET_STATS", "");
                strcpy(ctx->status_message, "Refreshing stats...");
            }
        } else if (selected_option == 1) {
            if (ctx->room_state == QUIZ_STATE_WAITING) {
                edit_mode = 1;
                sprintf(duration_input, "%d", ctx->quiz_duration);
            } else {
                strcpy(ctx->status_message, "Cannot change after quiz started");
            }
        } else if (selected_option == 2) {
            if (ctx->room_state == QUIZ_STATE_WAITING) {
                edit_mode = 2;
                strncpy(filename_input, ctx->question_file, sizeof(filename_input) - 1);
            } else {
                strcpy(ctx->status_message, "Cannot change after quiz started");
            }
        } else if (selected_option == 3) {
            // Leave room
            ctx->current_room_id = -1;
            ctx->is_host = false;
            ctx->room_state = QUIZ_STATE_WAITING;
            ctx->participant_count = 0;
            ctx->current_state = PAGE_DASHBOARD;
        }
    } else if (input == 's' || input == 'S') {
        if (ctx->room_state == QUIZ_STATE_WAITING) {
            client_send_message(ctx, "START_GAME", "");
            strcpy(ctx->status_message, "Starting quiz...");
        }
    } else if (input == 'r' || input == 'R') {
        client_send_message(ctx, "GET_STATS", "");
        strcpy(ctx->status_message, "Refreshing stats...");
        last_stats_request = now;
    } else if (input == 'd' || input == 'D') {
        if (ctx->room_state == QUIZ_STATE_WAITING) {
            edit_mode = 1;
            sprintf(duration_input, "%d", ctx->quiz_duration);
        }
    } else if (input == 'f' || input == 'F') {
        if (ctx->room_state == QUIZ_STATE_WAITING) {
            edit_mode = 2;
            strncpy(filename_input, ctx->question_file, sizeof(filename_input) - 1);
        }
    } else if (input == 'q' || input == 'Q') {
        ctx->current_room_id = -1;
        ctx->is_host = false;
        ctx->room_state = QUIZ_STATE_WAITING;
        ctx->participant_count = 0;
        ctx->current_state = PAGE_DASHBOARD;
    }
}
