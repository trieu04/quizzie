#include "client.h"
#include "ui.h"
#include "net.h"
#include <ncurses.h>

ClientContext* client_init() {
    ClientContext* ctx = (ClientContext*)malloc(sizeof(ClientContext));
    if (!ctx) {
        perror("malloc");
        return NULL;
    }
    ctx->socket_fd = -1;
    ctx->running = true;
    return ctx;
}

void client_cleanup(ClientContext* ctx) {
    if (ctx) {
        if (ctx->socket_fd != -1) {
            close(ctx->socket_fd);
        }
        free(ctx);
    }
}

void client_run(ClientContext* ctx) {
    // Simple event loop
    // In a real app, this would use select() or poll() to handle both UI input and network data
    // For now, we just wait for user input to exit
    
    ctx->current_state = PAGE_LOGIN;

    while (ctx->running) {
        // Clear screen at start of frame
        erase();

        // Dispatch draw based on state
        switch (ctx->current_state) {
            case PAGE_LOGIN:
                page_login_draw(ctx);
                break;
            case PAGE_DASHBOARD:
                page_dashboard_draw(ctx);
                break;
            case PAGE_ROOM_LIST:
                page_room_list_draw(ctx);
                break;
            case PAGE_QUIZ:
                page_quiz_draw(ctx);
                break;
        }

        refresh();

        // Get input
        int ch = ui_get_input();

        // Global keys
        if (ch == KEY_F(10)) {
            ctx->running = false;
            continue;
        }

        // Dispatch input based on state
        switch (ctx->current_state) {
            case PAGE_LOGIN:
                page_login_handle_input(ctx, ch);
                break;
            case PAGE_DASHBOARD:
                page_dashboard_handle_input(ctx, ch);
                break;
            case PAGE_ROOM_LIST:
                page_room_list_handle_input(ctx, ch);
                break;
            case PAGE_QUIZ:
                page_quiz_handle_input(ctx, ch);
                break;
        }
    }
}
