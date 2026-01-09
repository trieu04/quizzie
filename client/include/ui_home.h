#ifndef UI_HOME_H
#define UI_HOME_H

#include <gtk/gtk.h>

void ui_show_home_window(GtkWidget **window_out, GtkWidget **status_label_out, const char *username);
void home_controller_on_logout();

#endif
