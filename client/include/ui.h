#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>

void ui_init(int* argc, char*** argv);
void ui_show_login();
void ui_show_home(const char* username);
int ui_get_socket();

#endif
