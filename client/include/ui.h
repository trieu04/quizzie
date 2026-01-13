#ifndef UI_H
#define UI_H

#include "cJSON.h"
#include <gtk/gtk.h>

void ui_init(int* argc, char*** argv);
void ui_show_login();
void login_controller_on_login(const char* ip, int port, const char* username, const char* password);
void login_controller_on_register(const char* ip, int port, const char* username, const char* password);
void ui_show_home(const char* username);
void ui_show_room_detail(const char* room_id);
void ui_show_exam(const char* room_id, cJSON* questions, int* answers, long start_time, int duration_minutes);
int ui_get_socket();
const char* ui_get_username();

#endif
