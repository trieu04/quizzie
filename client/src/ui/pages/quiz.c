#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>
#include <time.h>

static int scroll_offset = 0;
static int selected_option = -1; // -1: none, 0-3: A-D

void page_quiz_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Sidebar (Left 25%)
    int sidebar_width = col / 4;
    for (int i = 0; i < row - 1; ++i) {
        mvaddch(i, sidebar_width, ACS_VLINE);
    }

    // Sidebar content
    attron(A_BOLD);
    mvprintw(1, 2, "QUIZ");
    attroff(A_BOLD);

    mvprintw(3, 2, "Room: %d", ctx->current_room_id);
    mvprintw(4, 2, "Player: %s", ctx->username);

    // Timer display
    if (ctx->quiz_start_time > 0 && ctx->quiz_duration > 0) {
        time_t now = time(NULL);
        int elapsed = (int)(now - ctx->quiz_start_time);
        int remaining = ctx->quiz_duration - elapsed;
        if (remaining < 0) remaining = 0;
        
        int mins = remaining / 60;
        int secs = remaining % 60;
        
        attron(A_BOLD);
        if (remaining < 60) {
            attron(A_BLINK);  // Blink when less than 1 minute
        }
        mvprintw(6, 2, "Time: %02d:%02d", mins, secs);
        attroff(A_BLINK);
        attroff(A_BOLD);
        
        // Auto-submit when time is up
        if (remaining == 0 && ctx->question_count > 0) {
            // Build answer string and submit
            char answer_str[MAX_QUESTIONS + 1];
            for (int i = 0; i < ctx->question_count; i++) {
                answer_str[i] = ctx->answers[i] ? ctx->answers[i] : '-';
            }
            answer_str[ctx->question_count] = '\0';
            client_send_message(ctx, "SUBMIT", answer_str);
            strcpy(ctx->status_message, "Time's up! Auto-submitting...");
        }
    }

    mvprintw(8, 2, "Progress:");
    mvprintw(9, 2, "%d / %d", ctx->current_question + 1, ctx->question_count > 0 ? ctx->question_count : 1);

    // Question navigation in sidebar
    mvprintw(11, 2, "Questions:");
    int max_q_display = row - 15;
    for (int i = 0; i < ctx->question_count && i < max_q_display; ++i) {
        if (i == ctx->current_question) attron(A_REVERSE);
        char answered = ctx->answers[i] != 0 ? ctx->answers[i] : '-';
        mvprintw(13 + i, 4, "Q%d [%c]", i + 1, answered);
        if (i == ctx->current_question) attroff(A_REVERSE);
    }

    // Main Content (Right 75%)
    int main_x = sidebar_width + 2;
    int main_width = col - main_x - 2;

    // Status message at top
    if (strlen(ctx->status_message) > 0) {
        mvprintw(1, main_x, "%s", ctx->status_message);
    }

    // Check if we have questions
    if (ctx->question_count == 0) {
        attron(A_BOLD);
        if (ctx->quiz_available) {
            // Quiz is available, client can start
            mvprintw(row / 2 - 2, main_x, "Quiz is ready!");
            attroff(A_BOLD);
            mvprintw(row / 2, main_x, "Press 'S' to start your quiz.");
            mvprintw(row / 2 + 1, main_x, "Time limit: %d seconds", ctx->quiz_duration);
        } else {
            // Waiting for host
            mvprintw(row / 2 - 2, main_x, "Waiting for quiz to start...");
            attroff(A_BOLD);
            mvprintw(row / 2, main_x, "Waiting for host to start the game...");
        }
    }
    else {
        // Display current question
        Question* q = &ctx->questions[ctx->current_question];

        // Question number and text
        attron(A_BOLD);
        mvprintw(3, main_x, "Question %d:", ctx->current_question + 1);
        attroff(A_BOLD);

        // Word wrap the question text
        int y = 5;
        int len = strlen(q->question);
        int pos = 0;
        while (pos < len && y < row - 10) {
            int line_len = (len - pos > main_width) ? main_width : (len - pos);
            // Find word boundary for cleaner wrap
            if (line_len < len - pos) {
                int space = line_len;
                while (space > 0 && q->question[pos + space] != ' ') space--;
                if (space > 0) line_len = space + 1;
            }
            mvprintw(y++, main_x, "%.*s", line_len, q->question + pos);
            pos += line_len;
        }

        // Options
        y += 2;
        char options[] = { 'A', 'B', 'C', 'D' };
        for (int i = 0; i < 4; i++) {
            if (ctx->answers[ctx->current_question] == options[i]) {
                attron(A_REVERSE);
            }
            if (i == selected_option) {
                attron(A_BOLD);
            }
            mvprintw(y + i * 2, main_x, "[%c] %s", options[i], q->options[i]);
            attroff(A_REVERSE);
            attroff(A_BOLD);
        }

        // Navigation hints
        y = row - 4;
        mvprintw(y, main_x, "Selected: %c",
            ctx->answers[ctx->current_question] ? ctx->answers[ctx->current_question] : '-');
    }

    // Footer
    if (ctx->question_count > 0) {
        mvprintw(row - 2, 2, "A-D: Answer | LEFT/RIGHT: Nav | S: Submit | ESC: Leave");
    } else if (ctx->quiz_available) {
        mvprintw(row - 2, 2, "S: Start Quiz | ESC: Leave | F10: Quit");
    } else {
        mvprintw(row - 2, 2, "Waiting for host... | ESC: Leave | F10: Quit");
    }
}

void page_quiz_handle_input(ClientContext* ctx, int input) {
    if (input == 27) { // ESC
        // Leave quiz - go back to dashboard
        ctx->current_room_id = -1;
        ctx->question_count = 0;
        ctx->current_question = 0;
        ctx->is_host = false;
        ctx->quiz_available = false;
        ctx->quiz_start_time = 0;
        memset(ctx->answers, 0, sizeof(ctx->answers));
        ctx->current_state = PAGE_DASHBOARD;
        scroll_offset = 0;
        selected_option = -1;
    }
    else if (input == 's' || input == 'S') {
        if (ctx->question_count == 0 && ctx->quiz_available) {
            // Client starts their quiz (requests questions from server)
            if (ctx->connected) {
                client_send_message(ctx, "CLIENT_START", "");
                strcpy(ctx->status_message, "Starting your quiz...");
            }
        }
        else if (ctx->question_count > 0) {
            // Submit answers
            if (ctx->connected) {
                // Build answer string
                char answer_str[MAX_QUESTIONS + 1];
                for (int i = 0; i < ctx->question_count; i++) {
                    answer_str[i] = ctx->answers[i] ? ctx->answers[i] : '-';
                }
                answer_str[ctx->question_count] = '\0';

                client_send_message(ctx, "SUBMIT", answer_str);
                strcpy(ctx->status_message, "Answers submitted!");
            }
        }
    }
    else if (input == KEY_LEFT) {
        if (ctx->current_question > 0) {
            ctx->current_question--;
            selected_option = -1;
        }
    }
    else if (input == KEY_RIGHT) {
        if (ctx->current_question < ctx->question_count - 1) {
            ctx->current_question++;
            selected_option = -1;
        }
    }
    else if (input == KEY_UP) {
        if (scroll_offset > 0) scroll_offset--;
    }
    else if (input == KEY_DOWN) {
        scroll_offset++;
    }
    else if ((input == 'a' || input == 'A') && ctx->question_count > 0) {
        ctx->answers[ctx->current_question] = 'A';
        selected_option = 0;
    }
    else if ((input == 'b' || input == 'B') && ctx->question_count > 0) {
        ctx->answers[ctx->current_question] = 'B';
        selected_option = 1;
    }
    else if ((input == 'c' || input == 'C') && ctx->question_count > 0) {
        ctx->answers[ctx->current_question] = 'C';
        selected_option = 2;
    }
    else if ((input == 'd' || input == 'D') && ctx->question_count > 0) {
        ctx->answers[ctx->current_question] = 'D';
        selected_option = 3;
    }
    else if (input == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            if (event.bstate & BUTTON1_CLICKED) {
                // Check sidebar clicks for question navigation
                int sidebar_width = getmaxx(stdscr) / 4;
                if (event.x < sidebar_width) {
                    for (int i = 0; i < ctx->question_count; ++i) {
                        if (event.y == 13 + i) {
                            ctx->current_question = i;
                            selected_option = -1;
                            break;
                        }
                    }
                }
            }
        }
    }
}
