#include "ui.h"
#include "ui_login.h"
#include "window_manager.h"
#include <stdlib.h>

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "8080"
#define LOGIN_WINDOW_WIDTH 400
#define LOGIN_WINDOW_HEIGHT 300
#define SPACING 5

typedef struct {
    GtkWidget *entry_ip;
    GtkWidget *entry_port;
    GtkWidget *entry_username;
    GtkWidget *entry_password;
} LoginWidgets;

static void on_login_btn_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    LoginWidgets *widgets = (LoginWidgets *)data;
    
    const char *ip = gtk_entry_get_text(GTK_ENTRY(widgets->entry_ip));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(widgets->entry_port));
    const char *username = gtk_entry_get_text(GTK_ENTRY(widgets->entry_username));
    const char *password = gtk_entry_get_text(GTK_ENTRY(widgets->entry_password));
    
    int port = atoi(port_str);
    login_controller_on_login(ip, port, username, password);
}

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    free(data); // Free the LoginWidgets struct
}

void ui_show_login_window(GtkWidget **window_out, GtkWidget **status_label_out) {
    GtkWidget *window = create_window("Quizzie Login", LOGIN_WINDOW_WIDTH, LOGIN_WINDOW_HEIGHT);
    *window_out = window;

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), SPACING);
    gtk_grid_set_column_spacing(GTK_GRID(grid), SPACING);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(window), grid);

    LoginWidgets *widgets = malloc(sizeof(LoginWidgets));

    // IP & Port
    GtkWidget *lbl_ip = gtk_label_new("Server IP:");
    widgets->entry_ip = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widgets->entry_ip), DEFAULT_IP);

    GtkWidget *lbl_port = gtk_label_new("Port:");
    widgets->entry_port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(widgets->entry_port), DEFAULT_PORT);

    gtk_grid_attach(GTK_GRID(grid), lbl_ip, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_ip, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_port, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_port, 3, 0, 1, 1);
    
    // Username & Password
    GtkWidget *lbl_user = gtk_label_new("Username:");
    widgets->entry_username = gtk_entry_new();

    GtkWidget *lbl_pass = gtk_label_new("Password:");
    widgets->entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(widgets->entry_password), FALSE);

    gtk_grid_attach(GTK_GRID(grid), lbl_user, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_username, 1, 1, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_pass, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_password, 1, 2, 3, 1);

    // Login Button
    GtkWidget *btn_login = gtk_button_new_with_label("Login");
    g_signal_connect(btn_login, "clicked", G_CALLBACK(on_login_btn_clicked), widgets);
    
    // Clean up struct when window is destroyed
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), widgets);

    gtk_grid_attach(GTK_GRID(grid), btn_login, 1, 3, 2, 1);

    // Status Bar
    *status_label_out = gtk_label_new("Status: Disconnected");
    gtk_grid_attach(GTK_GRID(grid), *status_label_out, 0, 4, 5, 1);

    gtk_widget_show_all(window);
}
