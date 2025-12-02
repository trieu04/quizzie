#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>

static int selected_option = 0; // 0: Create Room, 1: View Rooms, 2: Logout

void page_dashboard_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Title
    attron(A_BOLD);
    mvprintw(3, (col - 10)/2, "DASHBOARD");
    attroff(A_BOLD);
    
    // Welcome message
    mvprintw(5, (col - 30)/2, "Welcome, %s!", ctx->username);
    
    // Connection status
    if (ctx->connected) {
        mvprintw(6, (col - 30)/2, "[Connected to %s:%d]", SERVER_IP, SERVER_PORT);
    } else {
        mvprintw(6, (col - 30)/2, "[Disconnected]");
    }
    
    // Status message
    if (strlen(ctx->status_message) > 0) {
        mvprintw(8, (col - strlen(ctx->status_message))/2, "%s", ctx->status_message);
    }

    // Menu options
    int menu_start_y = row/2 - 2;
    
    if (selected_option == 0) attron(A_REVERSE);
    mvprintw(menu_start_y, (col - 20)/2, "[ Create New Room ]");
    if (selected_option == 0) attroff(A_REVERSE);

    if (selected_option == 1) attron(A_REVERSE);
    mvprintw(menu_start_y + 2, (col - 20)/2, "[ View Rooms ]");
    if (selected_option == 1) attroff(A_REVERSE);

    if (selected_option == 2) attron(A_REVERSE);
    mvprintw(menu_start_y + 4, (col - 20)/2, "[ Logout ]");
    if (selected_option == 2) attroff(A_REVERSE);

    mvprintw(row - 2, 2, "UP/DOWN: Select | ENTER: Confirm | F10: Quit");
}

void page_dashboard_handle_input(ClientContext* ctx, int input) {
    if (input == KEY_UP) {
        selected_option = (selected_option - 1 + 3) % 3;
    } else if (input == KEY_DOWN) {
        selected_option = (selected_option + 1) % 3;
    } else if (input == '\n' || input == KEY_ENTER) {
        if (selected_option == 0) {
            // Create New Room - always creates a new room
            if (ctx->connected) {
                char data[100];
                sprintf(data, "%s", ctx->username);
                client_send_message(ctx, "CREATE_ROOM", data);
                strcpy(ctx->status_message, "Creating room...");
            } else {
                strcpy(ctx->status_message, "Not connected to server!");
            }
        } else if (selected_option == 1) {
            // View Rooms - show room list to join or rejoin
            ctx->current_state = PAGE_ROOM_LIST;
        } else if (selected_option == 2) {
            // Logout
            net_close(ctx);
            ctx->username[0] = '\0';
            ctx->current_room_id = -1;
            ctx->status_message[0] = '\0';
            ctx->current_state = PAGE_LOGIN;
        }
    }
}
