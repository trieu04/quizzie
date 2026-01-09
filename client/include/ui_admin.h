#ifndef UI_ADMIN_H
#define UI_ADMIN_H

#include <gtk/gtk.h>

struct cJSON;

void ui_show_admin_dashboard(GtkWidget** window, GtkWidget** status_label, const char* username);
void ui_admin_update_room_list(struct cJSON* rooms_array);

#endif
