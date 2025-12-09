#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *username_entry = NULL;
static GtkWidget *password_entry = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *login_button = NULL;

static void on_login_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    const char* username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char* password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    
    if (strlen(username) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Username cannot be empty!");
        return;
    }
    
    // Try to connect to server if not already connected
    if (!ctx->connected) {
        gtk_label_set_text(GTK_LABEL(status_label), "Connecting to server...");
        if (net_connect(ctx, SERVER_IP, SERVER_PORT) != 0) {
            gtk_label_set_text(GTK_LABEL(status_label), "Failed to connect to server!");
            return;
        }
    }
    
    // Store username in context
    strncpy(ctx->username, username, sizeof(ctx->username) - 1);
    ctx->username[sizeof(ctx->username) - 1] = '\0';
    
    // Login success - go to dashboard
    ctx->current_state = PAGE_DASHBOARD;
    strcpy(ctx->status_message, "Logged in successfully!");
    
    ui_navigate_to_page(PAGE_DASHBOARD);
}

static gboolean on_entry_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    if (event->keyval == GDK_KEY_Return) {
        on_login_clicked(NULL, data);
        return TRUE;
    }
    return FALSE;
}

GtkWidget* page_login_create(ClientContext* ctx) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(page, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page, GTK_ALIGN_CENTER);
    
    // Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>QUIZZIE</span>");
    gtk_box_pack_start(GTK_BOX(page), title, FALSE, FALSE, 10);
    
    GtkWidget *subtitle = gtk_label_new("Quiz Game Client");
    gtk_box_pack_start(GTK_BOX(page), subtitle, FALSE, FALSE, 0);
    
    // Connection status
    GtkWidget *conn_status = gtk_label_new(ctx->connected ? 
        "Connected to server" : "Not connected");
    gtk_box_pack_start(GTK_BOX(page), conn_status, FALSE, FALSE, 10);
    
    // Form container
    GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(form_box, 300, -1);
    
    // Username
    GtkWidget *username_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_widget_set_size_request(username_label, 100, -1);
    gtk_widget_set_halign(username_label, GTK_ALIGN_START);
    username_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(username_entry), 31);
    g_signal_connect(username_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
    gtk_box_pack_start(GTK_BOX(username_box), username_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(username_box), username_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(form_box), username_box, FALSE, FALSE, 0);
    
    // Password
    GtkWidget *password_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_widget_set_size_request(password_label, 100, -1);
    gtk_widget_set_halign(password_label, GTK_ALIGN_START);
    password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_max_length(GTK_ENTRY(password_entry), 31);
    g_signal_connect(password_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
    gtk_box_pack_start(GTK_BOX(password_box), password_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(password_box), password_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(form_box), password_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(page), form_box, FALSE, FALSE, 0);
    
    // Login button
    login_button = gtk_button_new_with_label("LOGIN");
    gtk_widget_set_size_request(login_button, 150, 40);
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(page), login_button, FALSE, FALSE, 10);
    
    // Status label
    status_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(page), status_label, FALSE, FALSE, 0);
    
    return page;
}

void page_login_update(ClientContext* ctx) {
    // This function is called when still on login page
    // Navigation is handled by ui.c
    (void)ctx;
}
