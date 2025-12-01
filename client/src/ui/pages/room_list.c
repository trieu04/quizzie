#include "ui.h"
#include "net.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

static char room_id_input[16] = {0};
static int focus = 0; // 0: room ID input, 1: join button
static char error_message[64] = {0};

void page_room_list_draw(ClientContext* ctx) {
    int row, col;
    getmaxyx(stdscr, row, col);

    // Title
    attron(A_BOLD);
    mvprintw(3, (col - 20)/2, "JOIN A ROOM");
    attroff(A_BOLD);
    
    // Status message
    if (strlen(ctx->status_message) > 0) {
        mvprintw(5, (col - strlen(ctx->status_message))/2, "%s", ctx->status_message);
    }
    
    // Instructions
    mvprintw(row/2 - 4, (col - 50)/2, "Enter the Room ID to join an existing quiz room:");
    
    // Room ID input
    mvprintw(row/2 - 1, (col - 30)/2, "Room ID: ");
    if (focus == 0) attron(A_REVERSE);
    mvprintw(row/2 - 1, (col - 30)/2 + 10, "%-10s", room_id_input);
    if (focus == 0) attroff(A_REVERSE);
    
    // Join button
    if (focus == 1) attron(A_REVERSE);
    mvprintw(row/2 + 2, (col - 10)/2, "[ JOIN ]");
    if (focus == 1) attroff(A_REVERSE);
    
    // Error message
    if (strlen(error_message) > 0) {
        attron(A_BOLD);
        mvprintw(row/2 + 4, (col - strlen(error_message))/2, "%s", error_message);
        attroff(A_BOLD);
    }
    
    // Current room info
    if (ctx->current_room_id > 0) {
        mvprintw(row/2 + 6, (col - 30)/2, "Current Room: %d", ctx->current_room_id);
    }

    mvprintw(row - 2, 2, "TAB: Switch focus | ENTER: Join | ESC: Back | F10: Quit");
}

void page_room_list_handle_input(ClientContext* ctx, int input) {
    if (input == 27) { // ESC
        error_message[0] = '\0';
        ctx->current_state = PAGE_DASHBOARD;
    } else if (input == '\t') {
        focus = (focus + 1) % 2;
    } else if (input == '\n' || input == KEY_ENTER) {
        if (focus == 1 || focus == 0) {
            // Try to join room
            if (strlen(room_id_input) == 0) {
                strcpy(error_message, "Please enter a Room ID!");
                focus = 0;
                return;
            }
            
            int room_id = atoi(room_id_input);
            if (room_id <= 0) {
                strcpy(error_message, "Invalid Room ID!");
                focus = 0;
                return;
            }
            
            if (!ctx->connected) {
                strcpy(error_message, "Not connected to server!");
                return;
            }
            
            // Send join room request
            client_send_message(ctx, "JOIN_ROOM", room_id_input);
            ctx->current_room_id = room_id;
            sprintf(ctx->status_message, "Joining room %d...", room_id);
            error_message[0] = '\0';
            
            // Go to quiz page and wait for questions
            ctx->current_state = PAGE_QUIZ;
        }
    } else if (input == KEY_BACKSPACE || input == 127) {
        if (focus == 0 && strlen(room_id_input) > 0) {
            room_id_input[strlen(room_id_input) - 1] = '\0';
        }
    } else if (input >= '0' && input <= '9') {
        if (focus == 0 && strlen(room_id_input) < 10) {
            size_t len = strlen(room_id_input);
            room_id_input[len] = input;
            room_id_input[len + 1] = '\0';
        }
    } else if (input == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            // Handle mouse clicks if needed
        }
    }
}
