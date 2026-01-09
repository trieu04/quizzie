#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <gtk/gtk.h>

GtkWidget* create_window(const char *title, int width, int height);
void clear_window_signals(GtkWidget *window);

#endif
