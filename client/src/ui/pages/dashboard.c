#include "ui.h"
#include <ncurses.h>

static int selected_option = 0; // 0: Join Room, 1: Logout

void page_dashboard_draw(ClientContext* ctx) {
    (void)ctx;
    int row, col;
    getmaxyx(stdscr, row, col);

    mvprintw(row/2 - 5, (col - 10)/2, "DASHBOARD");

    if (selected_option == 0) attron(A_REVERSE);
    mvprintw(row/2 - 1, (col - 20)/2, "[ Join Room ]");
    if (selected_option == 0) attroff(A_REVERSE);

    if (selected_option == 1) attron(A_REVERSE);
    mvprintw(row/2 + 1, (col - 20)/2, "[ Logout ]");
    if (selected_option == 1) attroff(A_REVERSE);

    mvprintw(row - 2, 2, "Use UP/DOWN to select, ENTER to confirm. F10 to Quit.");
}

void page_dashboard_handle_input(ClientContext* ctx, int input) {
    if (input == KEY_UP || input == KEY_DOWN) {
        selected_option = !selected_option;
    } else if (input == '\n' || input == KEY_ENTER) {
        if (selected_option == 0) {
            ctx->current_state = PAGE_ROOM_LIST;
        } else {
            ctx->current_state = PAGE_LOGIN;
        }
    }
}
