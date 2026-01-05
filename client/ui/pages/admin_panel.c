#include "ui.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>

static GtkWidget *status_label = NULL;
static GtkWidget *logs_view = NULL;

static void load_recent_logs(void) {
	if (!logs_view) return;
	const char* paths[] = {"data/logs.txt", "../data/logs.txt", "../../data/logs.txt", NULL};
	char buffer[4096];
	buffer[0] = '\0';

	for (int i = 0; paths[i] != NULL; i++) {
		FILE* f = fopen(paths[i], "r");
		if (!f) continue;
		if (fseek(f, 0, SEEK_END) == 0) {
			long sz = ftell(f);
			long start = sz > (long)(sizeof(buffer) - 1) ? sz - (long)(sizeof(buffer) - 1) : 0;
			fseek(f, start, SEEK_SET);
			size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
			buffer[n] = '\0';
		}
		fclose(f);
		break;
	}

	const char* text = buffer[0] ? buffer : "No logs yet.";
	GtkTextBuffer* tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(logs_view));
	gtk_text_buffer_set_text(tb, text, -1);
}

static void on_create_room_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	
	ctx->current_state = PAGE_CREATE_ROOM;
	ui_navigate_to_page(PAGE_CREATE_ROOM);
}

static void on_view_rooms_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	ctx->current_state = PAGE_ROOM_LIST;
	ctx->force_page_refresh = true;
}

static void on_upload_page_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	ctx->current_state = PAGE_ADMIN_UPLOAD;
	ui_navigate_to_page(PAGE_ADMIN_UPLOAD);
}

static void on_refresh_logs_clicked(GtkWidget *widget, gpointer data) {
	(void)widget; (void)data;
	load_recent_logs();
}



GtkWidget* create_admin_panel_page(ClientContext* ctx) {
	GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");
	status_label = NULL;
	logs_view = NULL;

	GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_widget_set_margin_top(header, 20);
	gtk_widget_set_margin_bottom(header, 10);

	GtkWidget *title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold' foreground='#1c2430'>[ADMIN] Admin Panel</span>");
	gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
	gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

	GtkWidget *subtitle = gtk_label_new(NULL);
	char subtitle_text[128];
	snprintf(subtitle_text, sizeof(subtitle_text), "<span size='large' foreground='#7f8c8d'>Logged in as: <b>%s</b></span>", ctx->username);
	gtk_label_set_markup(GTK_LABEL(subtitle), subtitle_text);
	gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "header-subtitle");
	gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
	gtk_widget_set_margin_start(content, 20);
	gtk_widget_set_margin_end(content, 20);
	gtk_widget_set_margin_bottom(content, 20);

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

	GtkWidget *room_frame = gtk_frame_new(NULL);
	GtkWidget *room_label_header = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(room_label_header), "<b>[ROOM] Room Management</b>");
	gtk_frame_set_label_widget(GTK_FRAME(room_frame), room_label_header);
	gtk_frame_set_shadow_type(GTK_FRAME(room_frame), GTK_SHADOW_NONE);
	gtk_style_context_add_class(gtk_widget_get_style_context(room_frame), "card");

	GtkWidget *room_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_start(room_box, 15);
	gtk_widget_set_margin_end(room_box, 15);
	gtk_widget_set_margin_top(room_box, 15);
	gtk_widget_set_margin_bottom(room_box, 15);

	GtkWidget *button_grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

	GtkWidget *create_room_button = gtk_button_new_with_label("+ Create Quiz Room");
	gtk_widget_set_size_request(create_room_button, -1, 45);
	g_signal_connect(create_room_button, "clicked", G_CALLBACK(on_create_room_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(create_room_button), "btn-primary");
	gtk_box_pack_start(GTK_BOX(button_grid), create_room_button, TRUE, TRUE, 0);

	GtkWidget *view_rooms_button = gtk_button_new_with_label("> View All Rooms");
	gtk_widget_set_size_request(view_rooms_button, -1, 45);
	g_signal_connect(view_rooms_button, "clicked", G_CALLBACK(on_view_rooms_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(view_rooms_button), "btn-secondary");
	gtk_box_pack_start(GTK_BOX(button_grid), view_rooms_button, TRUE, TRUE, 0);

	GtkWidget *upload_page_button = gtk_button_new_with_label("📄 Preview Question Banks");
	gtk_widget_set_size_request(upload_page_button, -1, 45);
	g_signal_connect(upload_page_button, "clicked", G_CALLBACK(on_upload_page_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(upload_page_button), "btn-secondary");
	gtk_box_pack_start(GTK_BOX(button_grid), upload_page_button, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(room_box), button_grid, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(room_frame), room_box);
	gtk_box_pack_start(GTK_BOX(content), room_frame, FALSE, FALSE, 0);

	GtkWidget *logs_frame = gtk_frame_new(NULL);
	GtkWidget *logs_header = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(logs_header), "<b>[LOGS] Recent Activity</b>");
	gtk_frame_set_label_widget(GTK_FRAME(logs_frame), logs_header);
	gtk_frame_set_shadow_type(GTK_FRAME(logs_frame), GTK_SHADOW_NONE);
	gtk_style_context_add_class(gtk_widget_get_style_context(logs_frame), "card");

	GtkWidget *logs_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(logs_box, 12);
	gtk_widget_set_margin_end(logs_box, 12);
	gtk_widget_set_margin_top(logs_box, 12);
	gtk_widget_set_margin_bottom(logs_box, 12);

	GtkWidget *logs_toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *refresh_logs_btn = gtk_button_new_with_label("Refresh Logs");
	gtk_style_context_add_class(gtk_widget_get_style_context(refresh_logs_btn), "btn-secondary");
	g_signal_connect(refresh_logs_btn, "clicked", G_CALLBACK(on_refresh_logs_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(logs_toolbar), refresh_logs_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(logs_box), logs_toolbar, FALSE, FALSE, 0);

	GtkWidget *logs_scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(logs_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(logs_scrolled, -1, 200);
	logs_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(logs_view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(logs_view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(logs_view), GTK_WRAP_WORD_CHAR);
	gtk_style_context_add_class(gtk_widget_get_style_context(logs_view), "entry");
	gtk_container_add(GTK_CONTAINER(logs_scrolled), logs_view);
	gtk_box_pack_start(GTK_BOX(logs_box), logs_scrolled, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(logs_frame), logs_box);
	gtk_box_pack_start(GTK_BOX(content), logs_frame, TRUE, TRUE, 0);



	gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

	return main_box;
}

void page_admin_panel_update(ClientContext* ctx) {
	if (status_label && ctx->status_message[0] != '\0') {
		gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
	}
}
