#include "ui.h"
#include <stdio.h>

void ui_init(int *argc, char ***argv) {
    gtk_init(argc, argv);
    printf("UI initialized.\n");
}

void ui_show_login() {
    printf("Showing login screen...\n");
    // Placeholder for actual GTK window creation
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Quizzie Login");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
}
