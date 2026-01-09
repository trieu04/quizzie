#ifndef UI_LOGIN_H
#define UI_LOGIN_H

#include <gtk/gtk.h>

void ui_show_login_window(GtkWidget **window_out, GtkWidget **status_label_out);
void login_controller_on_login(const char *ip, int port, const char *username, const char *password);

#endif
