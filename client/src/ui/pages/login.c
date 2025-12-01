#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>

static char username[32] = {0};
static char password[32] = {0};
static int focus = 0; // 0: username, 1: password, 2: login button
static char error_message[64] = {0};

void page_login_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Title
    attron(A_BOLD);
    mvprintw(row/2 - 7, (col - 20)/2, "QUIZZIE - Login");
    attroff(A_BOLD);
    
    // Connection status
    if (ctx->connected) {
        attron(COLOR_PAIR(1)); // If colors are enabled
        mvprintw(row/2 - 5, (col - 30)/2, "[Connected to server]");
        attroff(COLOR_PAIR(1));
    } else {
        mvprintw(row/2 - 5, (col - 30)/2, "[Not connected]");
    }

    mvprintw(row/2 - 2, (col - 30)/2, "Username: ");
    if (focus == 0) attron(A_REVERSE);
    mvprintw(row/2 - 2, (col - 30)/2 + 10, "%-20s", username);
    if (focus == 0) attroff(A_REVERSE);

    mvprintw(row/2, (col - 30)/2, "Password: ");
    if (focus == 1) attron(A_REVERSE);
    // Mask password
    char masked[32];
    memset(masked, '*', strlen(password));
    masked[strlen(password)] = '\0';
    mvprintw(row/2, (col - 30)/2 + 10, "%-20s", masked);
    if (focus == 1) attroff(A_REVERSE);

    if (focus == 2) attron(A_REVERSE);
    mvprintw(row/2 + 3, (col - 10)/2, "[ LOGIN ]");
    if (focus == 2) attroff(A_REVERSE);

    // Error message
    if (strlen(error_message) > 0) {
        attron(A_BOLD);
        mvprintw(row/2 + 5, (col - strlen(error_message))/2, "%s", error_message);
        attroff(A_BOLD);
    }

    mvprintw(row - 2, 2, "TAB: Switch focus | ENTER: Select | F10: Quit");
}

void page_login_handle_input(ClientContext* ctx, int input) {
    if (input == '\t') {
        focus = (focus + 1) % 3;
        error_message[0] = '\0';
    } else if (input == '\n' || input == KEY_ENTER) {
        if (focus == 2) {
            // Validate input
            if (strlen(username) == 0) {
                strcpy(error_message, "Username cannot be empty!");
                focus = 0;
                return;
            }
            
            // Try to connect to server if not already connected
            if (!ctx->connected) {
                if (net_connect(ctx, SERVER_IP, SERVER_PORT) != 0) {
                    strcpy(error_message, "Failed to connect to server!");
                    return;
                }
            }
            
            // Store username in context
            strncpy(ctx->username, username, sizeof(ctx->username) - 1);
            
            // Login success - go to dashboard
            ctx->current_state = PAGE_DASHBOARD;
            strcpy(ctx->status_message, "Logged in successfully!");
            
            // Clear for next login
            error_message[0] = '\0';
        }
    } else if (input == KEY_BACKSPACE || input == 127) {
        if (focus == 0 && strlen(username) > 0) {
            username[strlen(username)-1] = '\0';
        }
        if (focus == 1 && strlen(password) > 0) {
            password[strlen(password)-1] = '\0';
        }
    } else if (input >= 32 && input <= 126) {
        if (focus == 0 && strlen(username) < 31) {
            size_t len = strlen(username);
            username[len] = input;
            username[len+1] = '\0';
        }
        if (focus == 1 && strlen(password) < 31) {
            size_t len = strlen(password);
            password[len] = input;
            password[len+1] = '\0';
        }
    }
}
