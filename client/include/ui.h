#ifndef UI_H
#define UI_H

#include "client.h"

// Function prototypes
void ui_init();
void ui_cleanup();
void ui_run(ClientContext* ctx);
void ui_show_message(const char* msg);
int ui_get_input();

// Page interfaces
void page_login_draw(ClientContext* ctx);
void page_login_handle_input(ClientContext* ctx, int input);

void page_dashboard_draw(ClientContext* ctx);
void page_dashboard_handle_input(ClientContext* ctx, int input);

void page_room_list_draw(ClientContext* ctx);
void page_room_list_handle_input(ClientContext* ctx, int input);

void page_quiz_draw(ClientContext* ctx);
void page_quiz_handle_input(ClientContext* ctx, int input);

#endif // UI_H
