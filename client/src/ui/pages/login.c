#include "ui.h"
#include <ncurses.h>
#include <string.h>

static char username[32] = {0};
static char password[32] = {0};
static int focus = 0; // 0: username, 1: password, 2: login button

void page_login_draw(ClientContext* ctx) {
    (void)ctx;
    int row, col;
    getmaxyx(stdscr, row, col);

    mvprintw(row/2 - 5, (col - 10)/2, "LOGIN PAGE");

    mvprintw(row/2 - 2, (col - 30)/2, "Username: %s", username);
    if (focus == 0) mvprintw(row/2 - 2, (col - 30)/2 + 10 + strlen(username), "_");

    mvprintw(row/2, (col - 30)/2, "Password: %s", password); // Should mask in real app
    if (focus == 1) mvprintw(row/2, (col - 30)/2 + 10 + strlen(password), "_");

    if (focus == 2) attron(A_REVERSE);
    mvprintw(row/2 + 3, (col - 10)/2, "[ LOGIN ]");
    if (focus == 2) attroff(A_REVERSE);

    mvprintw(row - 2, 2, "Press TAB to switch focus, ENTER to select. F10 to Quit.");
}

void page_login_handle_input(ClientContext* ctx, int input) {
    if (input == '\t') {
        focus = (focus + 1) % 3;
    } else if (input == '\n' || input == KEY_ENTER) {
        if (focus == 2) {
            // Mock login success
            ctx->current_state = PAGE_DASHBOARD;
        }
    } else if (input == KEY_BACKSPACE || input == 127) {
        if (focus == 0 && strlen(username) > 0) username[strlen(username)-1] = 0;
        if (focus == 1 && strlen(password) > 0) password[strlen(password)-1] = 0;
    } else if (input >= 32 && input <= 126) {
        if (focus == 0 && strlen(username) < 31) {
            int len = strlen(username);
            username[len] = input;
            username[len+1] = 0;
        }
        if (focus == 1 && strlen(password) < 31) {
            int len = strlen(password);
            password[len] = input;
            password[len+1] = 0;
        }
    }
    // Mouse handling could be added here
}
