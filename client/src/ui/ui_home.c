#include "ui_home.h"
#include "ui.h"
#include "window_manager.h"
#include <stdio.h>

static void on_logout_btn_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;
    home_controller_on_logout();
}

void ui_show_home_window(GtkWidget** window_out, GtkWidget** status_label_out, const char* username)
{
    char title[64];
    snprintf(title, sizeof(title), "Quizzie Home - %s", username);
    GtkWidget* window = create_window(title, 800, 600);
    *window_out = window;

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    char welcome_msg[64];
    snprintf(welcome_msg, sizeof(welcome_msg), "Welcome, %s!", username);
    GtkWidget* lbl_welcome = gtk_label_new(welcome_msg);
    // Increase font size
    PangoAttrList* attrlist = pango_attr_list_new();
    PangoAttribute* attr = pango_attr_scale_new(2.0); // Double size
    pango_attr_list_insert(attrlist, attr);
    gtk_label_set_attributes(GTK_LABEL(lbl_welcome), attrlist);
    pango_attr_list_unref(attrlist);

    GtkWidget* btn_logout = gtk_button_new_with_label("Logout");
    g_signal_connect(btn_logout, "clicked", G_CALLBACK(on_logout_btn_clicked), NULL);

    *status_label_out = gtk_label_new("Status: Connected");

    gtk_box_pack_start(GTK_BOX(vbox), lbl_welcome, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_logout, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), *status_label_out, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
}
