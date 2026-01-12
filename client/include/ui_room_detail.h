#ifndef UI_ROOM_DETAIL_H
#define UI_ROOM_DETAIL_H

#include <gtk/gtk.h>
#include "cJSON.h"

void ui_show_room_detail_window(GtkWidget** window_out, const char* room_id, const char* username);
void room_detail_controller_on_start_exam(const char* room_id);
void room_detail_controller_on_back();
void room_detail_update_info(cJSON* room_data);

#endif
