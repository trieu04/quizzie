#include "window_manager.h"

GtkWidget* create_window(const char *title, int width, int height) {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    return window;
}

void clear_window_signals(GtkWidget *window) {
    if (window) {
        g_signal_handlers_disconnect_by_func(window, G_CALLBACK(gtk_main_quit), NULL);
    }
}
