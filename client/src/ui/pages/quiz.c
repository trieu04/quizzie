#include "ui.h"
#include <ncurses.h>
#include <string.h>

static int current_question = 0;
static int scroll_offset = 0;
static const char* question_text = 
    "This is a very long question text to demonstrate scrolling capabilities in the Ncurses UI.\n"
    "It spans multiple lines and requires the user to scroll down to read the full content.\n"
    "Question: What is the capital of France?\n"
    "\n"
    "A. London\n"
    "B. Berlin\n"
    "C. Paris\n"
    "D. Madrid\n"
    "\n"
    "Please select the correct answer using the keys A, B, C, or D.\n"
    "You can also click on the options if mouse support is enabled.\n"
    "Use UP/DOWN keys to scroll this text area.";

void page_quiz_draw(ClientContext* ctx) {
    (void)ctx;
    int row, col;
    getmaxyx(stdscr, row, col);

    // Sidebar (Left 20%)
    int sidebar_width = col / 5;
    for (int i = 0; i < row; ++i) {
        mvaddch(i, sidebar_width, '|');
    }

    // Sidebar content
    mvprintw(1, 2, "Time: 10:00");
    mvprintw(3, 2, "Progress: 1/10");
    
    mvprintw(5, 2, "Questions:");
    for (int i = 0; i < 10; ++i) {
        if (i == current_question) attron(A_REVERSE);
        mvprintw(7 + i, 4, "Q%d", i + 1);
        if (i == current_question) attroff(A_REVERSE);
    }

    // Main Content (Right 80%)
    int main_x = sidebar_width + 2;
    int main_width = col - main_x - 2;
    
    // Simple scrolling text view
    int line = 0;
    int y = 2;
    const char* p = question_text;
    while (*p && y < row - 2) {
        // Find end of line
        const char* eol = strchr(p, '\n');
        int len = eol ? (int)(eol - p) : (int)strlen(p);
        
        if (line >= scroll_offset) {
            mvprintw(y++, main_x, "%.*s", len > main_width ? main_width : len, p);
        }
        
        if (!eol) break;
        p = eol + 1;
        line++;
    }

    mvprintw(row - 1, main_x, "Scroll: UP/DOWN | Select: A-D | Back: ESC");
}

void page_quiz_handle_input(ClientContext* ctx, int input) {
    if (input == 27) { // ESC
        ctx->current_state = PAGE_ROOM_LIST;
    } else if (input == KEY_UP) {
        if (scroll_offset > 0) scroll_offset--;
    } else if (input == KEY_DOWN) {
        scroll_offset++; // Should check bounds
    } else if (input == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            if (event.bstate & BUTTON1_CLICKED) {
                // Check sidebar clicks
                if (event.x < getmaxx(stdscr)/5) {
                    for (int i = 0; i < 10; ++i) {
                        if (event.y == 7 + i) {
                            current_question = i;
                            break;
                        }
                    }
                }
            }
        }
    }
}
