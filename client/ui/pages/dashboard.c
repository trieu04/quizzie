#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

static GtkWidget *status_label = NULL;

static void on_practice_page_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    if (!ctx) return;
    ctx->current_state = PAGE_PRACTICE;
    ui_navigate_to_page(PAGE_PRACTICE);
}

// Removed unused on_create_room_clicked to eliminate -Wunused-function warning

static void on_view_rooms_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_ROOM_LIST;
    ui_navigate_to_page(PAGE_ROOM_LIST);
}

static void on_reconnect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->last_room_id > 0 && strlen(ctx->username) > 0) {
        char rejoin_msg[128];
        snprintf(rejoin_msg, sizeof(rejoin_msg), "%d,%s", ctx->last_room_id, ctx->username);
        client_send_message(ctx, "REJOIN_PARTICIPANT", rejoin_msg);
        snprintf(ctx->status_message, sizeof(ctx->status_message), 
                 "Attempting to reconnect to room %d...", ctx->last_room_id);
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    } else {
        strcpy(ctx->status_message, "No previous room to reconnect to.");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    }
}

static void on_view_history_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_HISTORY;
    ui_navigate_to_page(PAGE_HISTORY);
}

GtkWidget* page_dashboard_create(ClientContext* ctx) {
    // Reset statics to avoid stale widget pointers when navigating back
    status_label = NULL;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");
    
    // Header section
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(header, 20);
    gtk_widget_set_margin_bottom(header, 20);
    
    // Title with emoji
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold' foreground='#1c2430'>Participant Dashboard</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 5);
    
    // Welcome message
    GtkWidget *welcome_label = gtk_label_new(NULL);
    char welcome[128];
    snprintf(welcome, sizeof(welcome), "<span size='large' foreground='#27ae60'>Welcome, <b>%s</b>!</span>", ctx->username);
    gtk_label_set_markup(GTK_LABEL(welcome_label), welcome);
    gtk_style_context_add_class(gtk_widget_get_style_context(welcome_label), "header-subtitle");
    gtk_box_pack_start(GTK_BOX(header), welcome_label, FALSE, FALSE, 0);
    
    // Connection status
    GtkWidget *conn_label = gtk_label_new(NULL);
    char conn_text[128];
    gtk_label_set_markup(GTK_LABEL(conn_label), conn_text);
    gtk_box_pack_start(GTK_BOX(header), conn_label, FALSE, FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);
    
    // Content area
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(content, 30);
    gtk_widget_set_margin_end(content, 30);
    gtk_widget_set_margin_bottom(content, 30);
    
    // Status message frame
    GtkWidget *status_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(status_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_frame), "card");
    status_label = gtk_label_new(ctx->status_message);
    gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
    gtk_widget_set_margin_start(status_label, 10);
    gtk_widget_set_margin_end(status_label, 10);
    gtk_widget_set_margin_top(status_label, 8);
    gtk_widget_set_margin_bottom(status_label, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-bar");
    gtk_container_add(GTK_CONTAINER(status_frame), status_label);
    gtk_box_pack_start(GTK_BOX(content), status_frame, FALSE, FALSE, 0);
    
    // Practice Mode Section
    GtkWidget *practice_frame = gtk_frame_new(NULL);
    GtkWidget *practice_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(practice_header), "<b>[*] Practice Mode</b>");
    gtk_frame_set_label_widget(GTK_FRAME(practice_frame), practice_header);
    gtk_frame_set_shadow_type(GTK_FRAME(practice_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(practice_frame), "card");

    GtkWidget *practice_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(practice_box, 15);
    gtk_widget_set_margin_end(practice_box, 15);
    gtk_widget_set_margin_top(practice_box, 15);
    gtk_widget_set_margin_bottom(practice_box, 15);

    GtkWidget *practice_text = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(practice_text), "Practice with local banks offline.");
    gtk_label_set_line_wrap(GTK_LABEL(practice_text), TRUE);
    gtk_box_pack_start(GTK_BOX(practice_box), practice_text, FALSE, FALSE, 0);

    GtkWidget *practice_btn = gtk_button_new_with_label("Open Practice Page");
    gtk_widget_set_size_request(practice_btn, -1, 45);
    g_signal_connect(practice_btn, "clicked", G_CALLBACK(on_practice_page_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(practice_btn), "btn-primary");
    gtk_box_pack_start(GTK_BOX(practice_box), practice_btn, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(practice_frame), practice_box);
    gtk_box_pack_start(GTK_BOX(content), practice_frame, FALSE, FALSE, 0);
    
    // Online Quiz Section
    GtkWidget *quiz_frame = gtk_frame_new(NULL);
    GtkWidget *quiz_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(quiz_header), "<b>[+] Online Quiz</b>");
    gtk_frame_set_label_widget(GTK_FRAME(quiz_frame), quiz_header);
    gtk_frame_set_shadow_type(GTK_FRAME(quiz_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(quiz_frame), "card");
    
    GtkWidget *quiz_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(quiz_box, 15);
    gtk_widget_set_margin_end(quiz_box, 15);
    gtk_widget_set_margin_top(quiz_box, 15);
    gtk_widget_set_margin_bottom(quiz_box, 15);
    
    GtkWidget *view_btn = gtk_button_new_with_label(">> Browse Quiz Rooms");
    gtk_widget_set_size_request(view_btn, -1, 45);
    g_signal_connect(view_btn, "clicked", G_CALLBACK(on_view_rooms_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(view_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(quiz_box), view_btn, FALSE, FALSE, 0);
    
    // Reconnect button (only show if have last_room_id)
    if (ctx->last_room_id > 0) {
        GtkWidget *reconnect_btn = gtk_button_new_with_label("🔄 Reconnect to Last Room");
        gtk_widget_set_size_request(reconnect_btn, -1, 45);
        g_signal_connect(reconnect_btn, "clicked", G_CALLBACK(on_reconnect_clicked), ctx);
        gtk_style_context_add_class(gtk_widget_get_style_context(reconnect_btn), "btn-primary");
        gtk_box_pack_start(GTK_BOX(quiz_box), reconnect_btn, FALSE, FALSE, 0);
    }
    
    gtk_container_add(GTK_CONTAINER(quiz_frame), quiz_box);
    gtk_box_pack_start(GTK_BOX(content), quiz_frame, FALSE, FALSE, 0);
    
    // History Section
    GtkWidget *history_frame = gtk_frame_new(NULL);
    GtkWidget *history_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(history_header), "<b>[📊] Test History</b>");
    gtk_frame_set_label_widget(GTK_FRAME(history_frame), history_header);
    gtk_frame_set_shadow_type(GTK_FRAME(history_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(history_frame), "card");
    
    GtkWidget *history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(history_box, 15);
    gtk_widget_set_margin_end(history_box, 15);
    gtk_widget_set_margin_top(history_box, 15);
    gtk_widget_set_margin_bottom(history_box, 15);
    
    GtkWidget *history_text = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(history_text), "View your test results history.");
    gtk_label_set_line_wrap(GTK_LABEL(history_text), TRUE);
    gtk_box_pack_start(GTK_BOX(history_box), history_text, FALSE, FALSE, 0);
    
    GtkWidget *history_btn = gtk_button_new_with_label("View History");
    gtk_widget_set_size_request(history_btn, -1, 45);
    g_signal_connect(history_btn, "clicked", G_CALLBACK(on_view_history_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(history_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(history_box), history_btn, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(history_frame), history_box);
    gtk_box_pack_start(GTK_BOX(content), history_frame, FALSE, FALSE, 0);
    

    
    gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);
    
    return main_box;
}

void page_dashboard_update(ClientContext* ctx) {
    if (status_label && strlen(ctx->status_message) > 0) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}
