#ifndef UI_H
#define UI_H

#include "cJSON.h"
#include <gtk/gtk.h>

void ui_init(int* argc, char*** argv);
void ui_show_login();
void ui_show_home(const char* username);
void ui_show_room_detail(const char* room_id);
void ui_show_exam(const char* room_id, cJSON* questions, int* answers, long start_time, int duration_minutes);
int ui_get_socket();
const char* ui_get_username();

#endif
