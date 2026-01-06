#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>

static GtkWidget *my_rooms_box = NULL;
static int last_room_count = -1;
static int last_my_room_count = -1;

static void on_create_room_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	
	ctx->current_state = PAGE_CREATE_ROOM;
	ui_navigate_to_page(PAGE_CREATE_ROOM);
}

static void on_upload_page_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	ctx->current_state = PAGE_FILE_PREVIEW;
	ui_navigate_to_page(PAGE_FILE_PREVIEW);
}

static void on_manage_room_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	
	if (!ctx->connected) {
		return;
	}
	
	// Rejoin as host to manage room
	client_send_message(ctx, "REJOIN_HOST", ctx->username);
}

static void on_delete_room_clicked(GtkWidget *widget, gpointer data) {
	int room_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "room_id"));
	ClientContext* ctx = (ClientContext*)data;
	
	if (!ctx->connected) {
		return;
	}
	
	// Send delete room request
	char msg[32];
	sprintf(msg, "%d", room_id);
	client_send_message(ctx, "DELETE_ROOM", msg);
	
	// Request updated list after short delay
	if (ctx->connected) {
		client_send_message(ctx, "LIST_ROOMS", "");
	}
}

static void on_refresh_rooms_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	
	if (ctx->connected) {
		client_send_message(ctx, "LIST_ROOMS", "");
	}
}

static void update_my_rooms_list(ClientContext* ctx) {
	if (!my_rooms_box) return;
	
	// Clear existing items
	GList *children = gtk_container_get_children(GTK_CONTAINER(my_rooms_box));
	for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
		gtk_widget_destroy(GTK_WIDGET(iter->data));
	}
	g_list_free(children);
	
	// Count my rooms
	int my_room_count = 0;
	for (int i = 0; i < ctx->room_count; i++) {
		if (ctx->rooms[i].is_my_room) my_room_count++;
	}
	
	if (my_room_count == 0) {
		GtkWidget *no_rooms = gtk_label_new("You haven't created any rooms yet.");
		gtk_style_context_add_class(gtk_widget_get_style_context(no_rooms), "dim-label");
		gtk_box_pack_start(GTK_BOX(my_rooms_box), no_rooms, FALSE, FALSE, 5);
		gtk_widget_show_all(my_rooms_box);
		return;
	}
	
	// Add my rooms
	for (int i = 0; i < ctx->room_count; i++) {
		RoomInfo* r = &ctx->rooms[i];
		if (!r->is_my_room) continue;
		
		GtkWidget *room_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_widget_set_margin_start(room_row, 5);
		gtk_widget_set_margin_end(room_row, 5);
		gtk_widget_set_margin_top(room_row, 5);
		gtk_widget_set_margin_bottom(room_row, 5);
		gtk_style_context_add_class(gtk_widget_get_style_context(room_row), "card");
		
		const char* state_str = r->state == 0 ? "Waiting" : 
		                       r->state == 1 ? "Active" : "Finished";
		const char* badge_class = r->state == 0 ? "badge-info" : 
		                          r->state == 1 ? "badge-warn" : "badge-success";
		
		char room_info[256];
		snprintf(room_info, sizeof(room_info), "Room #%d | %d participant(s)", 
		         r->id, r->player_count);
		
		GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		
		GtkWidget *label = gtk_label_new(room_info);
		gtk_widget_set_halign(label, GTK_ALIGN_START);
		gtk_style_context_add_class(gtk_widget_get_style_context(label), "room-label");
		gtk_box_pack_start(GTK_BOX(info_box), label, FALSE, FALSE, 0);
		
		GtkWidget *badge = gtk_label_new(state_str);
		gtk_widget_set_halign(badge, GTK_ALIGN_START);
		gtk_style_context_add_class(gtk_widget_get_style_context(badge), "badge");
		gtk_style_context_add_class(gtk_widget_get_style_context(badge), badge_class);
		gtk_box_pack_start(GTK_BOX(info_box), badge, FALSE, FALSE, 0);
		
		gtk_box_pack_start(GTK_BOX(room_row), info_box, TRUE, TRUE, 0);
		
		// Buttons
		GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		
		GtkWidget *manage_btn = gtk_button_new_with_label("Manage");
		g_object_set_data(G_OBJECT(manage_btn), "room_id", GINT_TO_POINTER(r->id));
		g_signal_connect(manage_btn, "clicked", G_CALLBACK(on_manage_room_clicked), ctx);
		gtk_style_context_add_class(gtk_widget_get_style_context(manage_btn), "btn-primary");
		gtk_widget_set_size_request(manage_btn, 90, -1);
		gtk_box_pack_start(GTK_BOX(btn_box), manage_btn, FALSE, FALSE, 0);
		
		GtkWidget *delete_btn = gtk_button_new_with_label("Delete");
		g_object_set_data(G_OBJECT(delete_btn), "room_id", GINT_TO_POINTER(r->id));
		g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_delete_room_clicked), ctx);
		gtk_style_context_add_class(gtk_widget_get_style_context(delete_btn), "btn-secondary");
		gtk_widget_set_size_request(delete_btn, 90, -1);
		gtk_box_pack_start(GTK_BOX(btn_box), delete_btn, FALSE, FALSE, 0);
		
		gtk_box_pack_start(GTK_BOX(room_row), btn_box, FALSE, FALSE, 0);
		
		gtk_box_pack_start(GTK_BOX(my_rooms_box), room_row, FALSE, FALSE, 0);
	}
	
	gtk_widget_show_all(my_rooms_box);
}



GtkWidget* create_admin_panel_page(ClientContext* ctx) {
	GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");

	GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_widget_set_margin_top(header, 20);
	gtk_widget_set_margin_bottom(header, 10);

	GtkWidget *title = gtk_label_new("Admin Dashboard");
	gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
	gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

	GtkWidget *subtitle = gtk_label_new(NULL);
	char subtitle_text[128];
	snprintf(subtitle_text, sizeof(subtitle_text), "Logged in as %s", ctx->username);
	gtk_label_set_text(GTK_LABEL(subtitle), subtitle_text);
	gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "page-subtitle");
	gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
	gtk_widget_set_margin_start(content, 20);
	gtk_widget_set_margin_end(content, 20);
	gtk_widget_set_margin_bottom(content, 20);

	GtkWidget *room_frame = gtk_frame_new(NULL);
	GtkWidget *room_label_header = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(room_label_header), "<b>Room Management</b>");
	gtk_frame_set_label_widget(GTK_FRAME(room_frame), room_label_header);
	gtk_frame_set_shadow_type(GTK_FRAME(room_frame), GTK_SHADOW_NONE);
	gtk_style_context_add_class(gtk_widget_get_style_context(room_frame), "card");

	GtkWidget *room_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_start(room_box, 15);
	gtk_widget_set_margin_end(room_box, 15);
	gtk_widget_set_margin_top(room_box, 15);
	gtk_widget_set_margin_bottom(room_box, 15);

	GtkWidget *desc_label = gtk_label_new("Create and manage quiz rooms. Rooms persist until manually deleted.");
	gtk_label_set_line_wrap(GTK_LABEL(desc_label), TRUE);
	gtk_widget_set_halign(desc_label, GTK_ALIGN_START);
	gtk_style_context_add_class(gtk_widget_get_style_context(desc_label), "dim-label");
	gtk_box_pack_start(GTK_BOX(room_box), desc_label, FALSE, FALSE, 0);

	GtkWidget *button_grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

	GtkWidget *create_room_button = gtk_button_new_with_label("Create New Room");
	gtk_widget_set_size_request(create_room_button, -1, 45);
	g_signal_connect(create_room_button, "clicked", G_CALLBACK(on_create_room_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(create_room_button), "btn-primary");
	gtk_box_pack_start(GTK_BOX(button_grid), create_room_button, TRUE, TRUE, 0);

	GtkWidget *preview_banks_button = gtk_button_new_with_label("Preview Question Banks");
	gtk_widget_set_size_request(preview_banks_button, -1, 45);
	g_signal_connect(preview_banks_button, "clicked", G_CALLBACK(on_upload_page_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(preview_banks_button), "btn-secondary");
	gtk_box_pack_start(GTK_BOX(button_grid), preview_banks_button, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(room_box), button_grid, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(room_frame), room_box);
	gtk_box_pack_start(GTK_BOX(content), room_frame, FALSE, FALSE, 0);

	// My Rooms Section
	GtkWidget *my_rooms_frame = gtk_frame_new(NULL);
	GtkWidget *rooms_header = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(rooms_header), "<b>My Rooms</b>");
	gtk_frame_set_label_widget(GTK_FRAME(my_rooms_frame), rooms_header);
	gtk_frame_set_shadow_type(GTK_FRAME(my_rooms_frame), GTK_SHADOW_NONE);
	gtk_style_context_add_class(gtk_widget_get_style_context(my_rooms_frame), "card");

	GtkWidget *rooms_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(rooms_container, 12);
	gtk_widget_set_margin_end(rooms_container, 12);
	gtk_widget_set_margin_top(rooms_container, 12);
	gtk_widget_set_margin_bottom(rooms_container, 12);

	GtkWidget *rooms_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *refresh_rooms_btn = gtk_button_new_with_label("Refresh Rooms");
	gtk_style_context_add_class(gtk_widget_get_style_context(refresh_rooms_btn), "btn-secondary");
	g_signal_connect(refresh_rooms_btn, "clicked", G_CALLBACK(on_refresh_rooms_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(rooms_toolbar), refresh_rooms_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(rooms_container), rooms_toolbar, FALSE, FALSE, 0);

	GtkWidget *rooms_scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rooms_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(rooms_scrolled, -1, 250);
	my_rooms_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add(GTK_CONTAINER(rooms_scrolled), my_rooms_box);
	gtk_box_pack_start(GTK_BOX(rooms_container), rooms_scrolled, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(my_rooms_frame), rooms_container);
	gtk_box_pack_start(GTK_BOX(content), my_rooms_frame, TRUE, TRUE, 0);

	// Initial display of empty rooms list
	update_my_rooms_list(ctx);

	gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

	return main_box;
}

void page_admin_panel_update(ClientContext* ctx) {
	// Count my rooms to detect changes
	int my_room_count = 0;
	for (int i = 0; i < ctx->room_count; i++) {
		if (ctx->rooms[i].is_my_room) my_room_count++;
	}
	
	// Only update list if room count changed or force refresh
	if (ctx->room_count != last_room_count || my_room_count != last_my_room_count || ctx->force_page_refresh) {
		update_my_rooms_list(ctx);
		last_room_count = ctx->room_count;
		last_my_room_count = my_room_count;
	}
}
