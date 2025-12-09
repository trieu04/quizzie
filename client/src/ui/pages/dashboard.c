#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *status_label = NULL;

static void on_create_room_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected) {
        char msg[100];
        sprintf(msg, "%s", ctx->username);
        client_send_message(ctx, "CREATE_ROOM", msg);
        strcpy(ctx->status_message, "Creating room...");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    } else {
        strcpy(ctx->status_message, "Not connected to server!");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    }
}

static void on_view_rooms_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_ROOM_LIST;
    ui_navigate_to_page(PAGE_ROOM_LIST);
}

static void on_logout_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    net_close(ctx);
    ctx->username[0] = '\0';
    ctx->current_room_id = -1;
    ctx->status_message[0] = '\0';
    ctx->current_state = PAGE_LOGIN;
    ui_navigate_to_page(PAGE_LOGIN);
}

GtkWidget* page_dashboard_create(ClientContext* ctx) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(page, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page, GTK_ALIGN_CENTER);
    
    // Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='x-large' weight='bold'>DASHBOARD</span>");
    gtk_box_pack_start(GTK_BOX(page), title, FALSE, FALSE, 10);
    
    // Welcome message
    char welcome[128];
    snprintf(welcome, sizeof(welcome), "Welcome, %s!", ctx->username);
    GtkWidget *welcome_label = gtk_label_new(welcome);
    gtk_box_pack_start(GTK_BOX(page), welcome_label, FALSE, FALSE, 0);
    
    // Connection status
    char conn_text[128];
    if (ctx->connected) {
        snprintf(conn_text, sizeof(conn_text), "Connected to %s:%d", SERVER_IP, SERVER_PORT);
    } else {
        snprintf(conn_text, sizeof(conn_text), "Disconnected");
    }
    GtkWidget *conn_label = gtk_label_new(conn_text);
    gtk_box_pack_start(GTK_BOX(page), conn_label, FALSE, FALSE, 0);
    
    // Status message
    status_label = gtk_label_new(ctx->status_message);
    gtk_box_pack_start(GTK_BOX(page), status_label, FALSE, FALSE, 10);
    
    // Menu buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(button_box, 250, -1);
    
    GtkWidget *create_btn = gtk_button_new_with_label("Create New Room");
    gtk_widget_set_size_request(create_btn, -1, 50);
    g_signal_connect(create_btn, "clicked", G_CALLBACK(on_create_room_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(button_box), create_btn, FALSE, FALSE, 0);
    
    GtkWidget *view_btn = gtk_button_new_with_label("View Rooms");
    gtk_widget_set_size_request(view_btn, -1, 50);
    g_signal_connect(view_btn, "clicked", G_CALLBACK(on_view_rooms_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(button_box), view_btn, FALSE, FALSE, 0);
    
    GtkWidget *logout_btn = gtk_button_new_with_label("Logout");
    gtk_widget_set_size_request(logout_btn, -1, 50);
    g_signal_connect(logout_btn, "clicked", G_CALLBACK(on_logout_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(button_box), logout_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(page), button_box, FALSE, FALSE, 10);
    
    return page;
}

void page_dashboard_update(ClientContext* ctx) {
    if (status_label && strlen(ctx->status_message) > 0) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}
