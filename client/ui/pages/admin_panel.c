
#include "ui.h"
#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *status_label = NULL;
static GtkWidget *filename_entry = NULL;
static GtkWidget *csv_textview = NULL;

static void on_upload_csv_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	const char* filename = gtk_entry_get_text(GTK_ENTRY(filename_entry));

	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(csv_textview));
	GtkTextIter start, end;
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	char* csv_data = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	if (!filename || strlen(filename) == 0) {
		strcpy(ctx->status_message, "Please enter a filename!");
		if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
		g_free(csv_data);
		return;
	}

	if (!csv_data || strlen(csv_data) == 0) {
		strcpy(ctx->status_message, "Please enter CSV data!");
		if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
		g_free(csv_data);
		return;
	}

	char message[BUFFER_SIZE];
	snprintf(message, BUFFER_SIZE, "%s,%s", filename, csv_data);
	client_send_message(ctx, "UPLOAD_QUESTIONS", message);

	strcpy(ctx->status_message, "Uploading CSV...");
	if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);

	g_free(csv_data);
}

static void on_create_room_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	char message[128];
	snprintf(message, sizeof(message), "%s", ctx->username);
	client_send_message(ctx, "CREATE_ROOM", message);

	strcpy(ctx->status_message, "Creating room...");
	if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
}

static void on_view_rooms_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	ctx->current_state = PAGE_ROOM_LIST;
	ctx->force_page_refresh = true;
}

static void on_logout_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	ctx->current_state = PAGE_LOGIN;
	ctx->force_page_refresh = true;
	ctx->role = 0;
	strcpy(ctx->username, "");
}

GtkWidget* create_admin_panel_page(ClientContext* ctx) {
	GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_widget_set_margin_top(header, 20);
	gtk_widget_set_margin_bottom(header, 10);

	GtkWidget *title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold' foreground='#2c3e50'>[ADMIN] Admin Panel</span>");
	gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

	GtkWidget *subtitle = gtk_label_new(NULL);
	char subtitle_text[128];
	snprintf(subtitle_text, sizeof(subtitle_text), "<span size='large' foreground='#7f8c8d'>Logged in as: <b>%s</b></span>", ctx->username);
	gtk_label_set_markup(GTK_LABEL(subtitle), subtitle_text);
	gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
	gtk_widget_set_margin_start(content, 20);
	gtk_widget_set_margin_end(content, 20);
	gtk_widget_set_margin_bottom(content, 20);

	GtkWidget *status_frame = gtk_frame_new(NULL);
	status_label = gtk_label_new(ctx->status_message);
	gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
	gtk_widget_set_margin_start(status_label, 10);
	gtk_widget_set_margin_end(status_label, 10);
	gtk_widget_set_margin_top(status_label, 8);
	gtk_widget_set_margin_bottom(status_label, 8);
	gtk_container_add(GTK_CONTAINER(status_frame), status_label);
	gtk_box_pack_start(GTK_BOX(content), status_frame, FALSE, FALSE, 0);

	GtkWidget *upload_frame = gtk_frame_new(NULL);
	GtkWidget *upload_label_header = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(upload_label_header), "<b>[FILE] Upload Question Bank</b>");
	gtk_frame_set_label_widget(GTK_FRAME(upload_frame), upload_label_header);
	gtk_frame_set_shadow_type(GTK_FRAME(upload_frame), GTK_SHADOW_ETCHED_IN);

	GtkWidget *upload_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_start(upload_box, 15);
	gtk_widget_set_margin_end(upload_box, 15);
	gtk_widget_set_margin_top(upload_box, 15);
	gtk_widget_set_margin_bottom(upload_box, 15);

	GtkWidget *filename_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(filename_label), "<b>Filename:</b>");
	gtk_widget_set_halign(filename_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(upload_box), filename_label, FALSE, FALSE, 0);

	filename_entry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(filename_entry), "e.g., exam_bank_math.csv or practice_bank_network.csv");
	gtk_widget_set_margin_start(filename_entry, 10);
	gtk_box_pack_start(GTK_BOX(upload_box), filename_entry, FALSE, FALSE, 0);

	GtkWidget *csv_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(csv_label), "<b>CSV Data:</b> <span size='small' foreground='#7f8c8d'>(Format: id,question,A,B,C,D,answer)</span>");
	gtk_widget_set_halign(csv_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(upload_box), csv_label, FALSE, FALSE, 5);

	GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(scrolled_window, -1, 250);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);

	csv_textview = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(csv_textview), GTK_WRAP_WORD);
	gtk_widget_set_margin_start(csv_textview, 5);
	gtk_widget_set_margin_end(csv_textview, 5);
	gtk_widget_set_margin_top(csv_textview, 5);
	gtk_widget_set_margin_bottom(csv_textview, 5);
	gtk_container_add(GTK_CONTAINER(scrolled_window), csv_textview);
	gtk_widget_set_margin_start(scrolled_window, 10);
	gtk_box_pack_start(GTK_BOX(upload_box), scrolled_window, TRUE, TRUE, 0);

	GtkWidget *upload_button = gtk_button_new_with_label(">> Upload to Server");
	gtk_widget_set_size_request(upload_button, -1, 40);
	g_signal_connect(upload_button, "clicked", G_CALLBACK(on_upload_csv_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(upload_box), upload_button, FALSE, FALSE, 5);

	gtk_container_add(GTK_CONTAINER(upload_frame), upload_box);
	gtk_box_pack_start(GTK_BOX(content), upload_frame, TRUE, TRUE, 0);

	GtkWidget *room_frame = gtk_frame_new(NULL);
	GtkWidget *room_label_header = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(room_label_header), "<b>[ROOM] Room Management</b>");
	gtk_frame_set_label_widget(GTK_FRAME(room_frame), room_label_header);
	gtk_frame_set_shadow_type(GTK_FRAME(room_frame), GTK_SHADOW_ETCHED_IN);

	GtkWidget *room_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_start(room_box, 15);
	gtk_widget_set_margin_end(room_box, 15);
	gtk_widget_set_margin_top(room_box, 15);
	gtk_widget_set_margin_bottom(room_box, 15);

	GtkWidget *button_grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

	GtkWidget *create_room_button = gtk_button_new_with_label("+ Create Quiz Room");
	gtk_widget_set_size_request(create_room_button, -1, 45);
	g_signal_connect(create_room_button, "clicked", G_CALLBACK(on_create_room_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(button_grid), create_room_button, TRUE, TRUE, 0);

	GtkWidget *view_rooms_button = gtk_button_new_with_label("> View All Rooms");
	gtk_widget_set_size_request(view_rooms_button, -1, 45);
	g_signal_connect(view_rooms_button, "clicked", G_CALLBACK(on_view_rooms_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(button_grid), view_rooms_button, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(room_box), button_grid, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(room_frame), room_box);
	gtk_box_pack_start(GTK_BOX(content), room_frame, FALSE, FALSE, 0);

	GtkWidget *logout_button = gtk_button_new_with_label("<< Logout");
	gtk_widget_set_size_request(logout_button, -1, 40);
	g_signal_connect(logout_button, "clicked", G_CALLBACK(on_logout_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(content), logout_button, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

	return main_box;
}

void page_admin_panel_update(ClientContext* ctx) {
	if (status_label && ctx->status_message[0] != '\0') {
		gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
	}
}
