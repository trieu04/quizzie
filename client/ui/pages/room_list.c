#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

static GtkWidget *room_list_box = NULL;
static GtkWidget *room_id_entry = NULL;
static bool list_requested = false;
static GtkWidget *loading_label = NULL;
static int last_room_count = -1; 

static void on_join_room_clicked(GtkWidget *widget, gpointer data) {
    int room_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "room_id"));
    bool is_my_room = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "is_my_room"));
    ClientContext* ctx = (ClientContext*)data;
    
    if (!ctx->connected) {
        return;
    }
    
    if (is_my_room) {
        // Rejoin as host
        client_send_message(ctx, "REJOIN_HOST", ctx->username);
    } else {
        // Join as participant
        char msg[100];
        sprintf(msg, "%d,%s", room_id, ctx->username);
        client_send_message(ctx, "JOIN_ROOM", msg);
        ctx->current_room_id = room_id;
    }
    // Don't reset list_requested here - let server response handle navigation
}

static void on_join_by_id_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    const char* room_id_str = gtk_entry_get_text(GTK_ENTRY(room_id_entry));
    if (strlen(room_id_str) == 0) {
		return;
	}
	
	int room_id = atoi(room_id_str);
	if (room_id <= 0) {
		return;
	}
	
	if (!ctx->connected) {
		return;
	}
	
	char msg[100];
	sprintf(msg, "%d,%s", room_id, ctx->username);
	client_send_message(ctx, "JOIN_ROOM", msg);
	ctx->current_room_id = room_id;
    // Don't reset list_requested here - let server response handle navigation
}

static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected) {
        client_send_message(ctx, "LIST_ROOMS", "");
    }
}

static void on_back_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    list_requested = false;
    ctx->status_message[0] = '\0';
    AppState target = ctx->role == 1 ? PAGE_ADMIN_PANEL : PAGE_DASHBOARD;
    ctx->current_state = target;
    ui_navigate_to_page(target);
}

static void update_room_list(ClientContext* ctx) {
    if (!room_list_box) return;
    
    // Only update if room list actually changed
    if (last_room_count == ctx->room_count) {
        return;  // No change, don't rebuild UI
    }
    
    last_room_count = ctx->room_count;
    
    // Clear existing items
    GList *children = gtk_container_get_children(GTK_CONTAINER(room_list_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    loading_label = NULL; // previous pointer is invalid after destroy
    
    if (ctx->room_count == 0) {
        loading_label = gtk_label_new("No rooms available yet.");
        gtk_style_context_add_class(gtk_widget_get_style_context(loading_label), "status-bar");
        gtk_box_pack_start(GTK_BOX(room_list_box), loading_label, FALSE, FALSE, 5);
        gtk_widget_show_all(room_list_box);
        return;
    }
    
    // Add rooms
    for (int i = 0; i < ctx->room_count; i++) {
        RoomInfo* r = &ctx->rooms[i];
        
        GtkWidget *room_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start(room_row, 5);
        gtk_widget_set_margin_end(room_row, 5);
        gtk_widget_set_margin_top(room_row, 3);
        gtk_widget_set_margin_bottom(room_row, 3);
        gtk_style_context_add_class(gtk_widget_get_style_context(room_row), "list-row");
        
        char room_info[256];
        const char* state_str = r->state == 0 ? "Waiting" : 
                       r->state == 1 ? "In Progress" : "Finished";
        const char* badge_class = r->state == 0 ? "badge-info" : r->state == 1 ? "badge-warn" : "badge-success";
        const char* host_marker = r->is_my_room ? " (You)" : "";
        
        snprintf(room_info, sizeof(room_info), "Room %d | Host: %s%s | Players: %d",
             r->id, r->host_username, host_marker, r->player_count);

        GtkWidget *label = gtk_label_new(room_info);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(room_row), label, TRUE, TRUE, 0);

        GtkWidget *badge = gtk_label_new(state_str);
        gtk_style_context_add_class(gtk_widget_get_style_context(badge), "badge");
        gtk_style_context_add_class(gtk_widget_get_style_context(badge), badge_class);
        gtk_box_pack_start(GTK_BOX(room_row), badge, FALSE, FALSE, 0);
        
        GtkWidget *join_btn = gtk_button_new_with_label(r->is_my_room ? "Rejoin as Host" : "Join");
        g_object_set_data(G_OBJECT(join_btn), "room_id", GINT_TO_POINTER(r->id));
        g_object_set_data(G_OBJECT(join_btn), "is_my_room", GINT_TO_POINTER(r->is_my_room));
        g_signal_connect(join_btn, "clicked", G_CALLBACK(on_join_room_clicked), ctx);
        gtk_style_context_add_class(gtk_widget_get_style_context(join_btn), r->is_my_room ? "btn-secondary" : "btn-primary");
        gtk_box_pack_start(GTK_BOX(room_row), join_btn, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(room_list_box), room_row, FALSE, FALSE, 0);
    }
    
    gtk_widget_show_all(room_list_box);
}

GtkWidget* page_room_list_create(ClientContext* ctx) {
    // Reset statics to avoid stale pointers between navigations
    room_list_box = NULL;
    room_id_entry = NULL;
    loading_label = NULL;
    list_requested = false;
    last_room_count = -1;  // Force initial update

    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(page), "page");
    gtk_widget_set_margin_start(page, 20);
    gtk_widget_set_margin_end(page, 20);
    gtk_widget_set_margin_top(page, 20);
    gtk_widget_set_margin_bottom(page, 20);
    
    // Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='x-large' weight='bold'>AVAILABLE ROOMS</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(page), title, FALSE, FALSE, 10);
    
    // Toolbar
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *refresh_btn = gtk_button_new_with_label("Refresh");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(refresh_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(toolbar), refresh_btn, FALSE, FALSE, 0);
    
    GtkWidget *back_btn = gtk_button_new_with_label("Back to Dashboard");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(back_btn), "btn-ghost");
    gtk_box_pack_end(GTK_BOX(toolbar), back_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(page), toolbar, FALSE, FALSE, 5);
    
    // Room list in scrolled window
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled, -1, 300);
    gtk_style_context_add_class(gtk_widget_get_style_context(scrolled), "card");
    
    room_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled), room_list_box);
    gtk_box_pack_start(GTK_BOX(page), scrolled, TRUE, TRUE, 0);
    
    // Manual join section
    GtkWidget *manual_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *manual_label = gtk_label_new("Join by Room ID:");
    room_id_entry = gtk_entry_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(room_id_entry), "entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(room_id_entry), "Enter room ID");
    gtk_widget_set_size_request(room_id_entry, 150, -1);
    
    GtkWidget *join_btn = gtk_button_new_with_label("Join");
    g_signal_connect(join_btn, "clicked", G_CALLBACK(on_join_by_id_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(join_btn), "btn-primary");
    
    gtk_box_pack_start(GTK_BOX(manual_box), manual_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(manual_box), room_id_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(manual_box), join_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(page), manual_box, FALSE, FALSE, 10);
    
    // Request room list
    if (!list_requested && ctx->connected) {
        client_send_message(ctx, "LIST_ROOMS", "");
        list_requested = true;
        strcpy(ctx->status_message, "Loading rooms...");
    }
    
    update_room_list(ctx);
    
    return page;
}

void page_room_list_update(ClientContext* ctx) {
    update_room_list(ctx);
}
