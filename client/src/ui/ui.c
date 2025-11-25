#include "ui.h"
#include <ncurses.h>

void ui_init() {
    initscr();              // Start curses mode
    cbreak();               // Line buffering disabled
    noecho();               // Don't echo while we do getch
    keypad(stdscr, TRUE);   // Enable special keys
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL); // Enable mouse
    curs_set(0);            // Hide cursor
}

void ui_cleanup() {
    endwin();               // End curses mode
}

void ui_show_message(const char* msg) {
    printw("%s\n", msg);
    refresh();
}

int ui_get_input() {
    return getch();
}

void ui_run(ClientContext* ctx) {
    (void)ctx; // Unused for now
    // UI specific loop if needed
}
