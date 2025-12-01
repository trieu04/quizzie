#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>

static int selected_option = 0; // 0: Play Again, 1: Back to Dashboard

void page_result_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Title
    attron(A_BOLD);
    mvprintw(row/2 - 8, (col - 20)/2, "QUIZ RESULTS");
    attroff(A_BOLD);
    
    // Score display
    mvprintw(row/2 - 4, (col - 30)/2, "Your Score:");
    
    attron(A_BOLD);
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "%d / %d", ctx->score, ctx->total_questions);
    mvprintw(row/2 - 2, (col - strlen(score_str))/2, "%s", score_str);
    attroff(A_BOLD);
    
    // Percentage
    float percentage = ctx->total_questions > 0 ? 
                       (float)ctx->score / ctx->total_questions * 100 : 0;
    mvprintw(row/2, (col - 20)/2, "Percentage: %.1f%%", percentage);
    
    // Message based on score
    const char* message;
    if (percentage >= 80) {
        message = "Excellent! Great job!";
    } else if (percentage >= 60) {
        message = "Good work! Keep it up!";
    } else if (percentage >= 40) {
        message = "Not bad! Room for improvement.";
    } else {
        message = "Keep practicing!";
    }
    mvprintw(row/2 + 2, (col - strlen(message))/2, "%s", message);
    
    // Options
    int menu_y = row/2 + 5;
    
    if (selected_option == 0) attron(A_REVERSE);
    mvprintw(menu_y, (col - 20)/2, "[ Play Again ]");
    if (selected_option == 0) attroff(A_REVERSE);
    
    if (selected_option == 1) attron(A_REVERSE);
    mvprintw(menu_y + 2, (col - 20)/2, "[ Dashboard ]");
    if (selected_option == 1) attroff(A_REVERSE);

    mvprintw(row - 2, 2, "UP/DOWN: Select | ENTER: Confirm | F10: Quit");
}

void page_result_handle_input(ClientContext* ctx, int input) {
    if (input == KEY_UP) {
        selected_option = (selected_option - 1 + 2) % 2;
    } else if (input == KEY_DOWN) {
        selected_option = (selected_option + 1) % 2;
    } else if (input == '\n' || input == KEY_ENTER) {
        // Reset quiz state
        ctx->score = 0;
        ctx->question_count = 0;
        ctx->current_question = 0;
        memset(ctx->answers, 0, sizeof(ctx->answers));
        ctx->status_message[0] = '\0';
        
        if (selected_option == 0) {
            // Play Again - go back to room list
            ctx->current_room_id = -1;
            ctx->current_state = PAGE_ROOM_LIST;
        } else {
            // Dashboard
            ctx->current_room_id = -1;
            ctx->is_host = false;
            ctx->current_state = PAGE_DASHBOARD;
        }
        selected_option = 0;
    }
}
