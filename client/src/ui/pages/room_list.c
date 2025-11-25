#include "ui.h"
#include <ncurses.h>

#define MAX_ROOMS 5
static const char* rooms[MAX_ROOMS] = {
    "Room 101 - General Knowledge",
    "Room 102 - Science",
    "Room 103 - History",
    "Room 104 - Geography",
    "Room 105 - Sports"
};

void page_room_list_draw(ClientContext* ctx) {
    (void)ctx;
    int row, col;
    getmaxyx(stdscr, row, col);

    mvprintw(2, (col - 20)/2, "AVAILABLE ROOMS");

    for (int i = 0; i < MAX_ROOMS; ++i) {
        mvprintw(5 + i*2, 4, "[%d] %s", i+1, rooms[i]);
    }

    mvprintw(row - 2, 2, "Click on a room or press number key to join. ESC to Back.");
}

void page_room_list_handle_input(ClientContext* ctx, int input) {
    if (input == 27) { // ESC
        ctx->current_state = PAGE_DASHBOARD;
    } else if (input >= '1' && input <= '5') {
        // Join room
        ctx->current_state = PAGE_QUIZ;
    } else if (input == KEY_MOUSE) {
        MEVENT event;
        if (getmouse(&event) == OK) {
            if (event.bstate & BUTTON1_CLICKED) {
                for (int i = 0; i < MAX_ROOMS; ++i) {
                    if (event.y == 5 + i*2 && event.x >= 4 && event.x <= 50) {
                        ctx->current_state = PAGE_QUIZ;
                        break;
                    }
                }
            }
        }
    }
}
