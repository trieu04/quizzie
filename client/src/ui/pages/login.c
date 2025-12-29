
#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *username_entry = NULL;
static GtkWidget *password_entry = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *login_button = NULL;
static GtkWidget *register_button = NULL;

static void on_register_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	const char* username = gtk_entry_get_text(GTK_ENTRY(username_entry));
	const char* password = gtk_entry_get_text(GTK_ENTRY(password_entry));

	if (strlen(username) == 0 || strlen(password) == 0) {
		gtk_label_set_text(GTK_LABEL(status_label), "Username and password cannot be empty!");
		return;
	}

	if (!ctx->connected) {
		gtk_label_set_text(GTK_LABEL(status_label), "Connecting to server...");
		if (net_connect(ctx, SERVER_IP, SERVER_PORT) != 0) {
			gtk_label_set_text(GTK_LABEL(status_label), "Failed to connect to server!");
			return;
		}
	}

	char msg[128];
	snprintf(msg, sizeof(msg), "%s,%s", username, password);
	client_send_message(ctx, "REGISTER", msg);
	gtk_label_set_text(GTK_LABEL(status_label), "Registering...");
}

static void on_login_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	const char* username = gtk_entry_get_text(GTK_ENTRY(username_entry));
	const char* password = gtk_entry_get_text(GTK_ENTRY(password_entry));

	if (strlen(username) == 0 || strlen(password) == 0) {
		gtk_label_set_text(GTK_LABEL(status_label), "Username and password cannot be empty!");
		return;
	}

	if (!ctx->connected) {
		gtk_label_set_text(GTK_LABEL(status_label), "Connecting to server...");
		if (net_connect(ctx, SERVER_IP, SERVER_PORT) != 0) {
			gtk_label_set_text(GTK_LABEL(status_label), "Failed to connect to server!");
			return;
		}
	}

	strncpy(ctx->username, username, sizeof(ctx->username) - 1);
	ctx->username[sizeof(ctx->username) - 1] = '\0';

	char msg[128];
	snprintf(msg, sizeof(msg), "%s,%s", username, password);
	client_send_message(ctx, "LOGIN", msg);
	gtk_label_set_text(GTK_LABEL(status_label), "Logging in...");
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
	GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_top(header, 40);
	gtk_widget_set_margin_bottom(header, 30);

	GtkWidget *title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(title), "<span size='30000' weight='bold' foreground='#2c3e50'>QUIZZIE</span>");
	gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

	GtkWidget *subtitle = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(subtitle), "<span size='large' foreground='#7f8c8d'>Online Quiz Application</span>");
	gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
	gtk_widget_set_halign(content, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(content, GTK_ALIGN_CENTER);

	GtkWidget *form_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(form_frame), GTK_SHADOW_ETCHED_IN);
	GtkWidget *form_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(form_label), "<b>Login / Register</b>");
	gtk_frame_set_label_widget(GTK_FRAME(form_frame), form_label);

	GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
	gtk_widget_set_size_request(form_box, 350, -1);
	gtk_widget_set_margin_start(form_box, 25);
	gtk_widget_set_margin_end(form_box, 25);
	gtk_widget_set_margin_top(form_box, 20);
	gtk_widget_set_margin_bottom(form_box, 20);

	GtkWidget *username_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(username_label), "<b>Username:</b>");
	gtk_widget_set_halign(username_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(form_box), username_label, FALSE, FALSE, 0);

	username_entry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Enter your username");
	gtk_entry_set_max_length(GTK_ENTRY(username_entry), 31);
	gtk_widget_set_size_request(username_entry, -1, 35);
	g_signal_connect(username_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(form_box), username_entry, FALSE, FALSE, 0);

	GtkWidget *password_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(password_label), "<b>Password:</b>");
	gtk_widget_set_halign(password_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(form_box), password_label, FALSE, FALSE, 0);

	password_entry = gtk_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Enter your password");
	gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
	gtk_entry_set_max_length(GTK_ENTRY(password_entry), 31);
	gtk_widget_set_size_request(password_entry, -1, 35);
	g_signal_connect(password_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(form_box), password_entry, FALSE, FALSE, 0);

	GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_margin_top(button_box, 10);

	login_button = gtk_button_new_with_label("LOGIN");
	gtk_widget_set_size_request(login_button, -1, 45);
	g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(button_box), login_button, TRUE, TRUE, 0);

	register_button = gtk_button_new_with_label("REGISTER");
	gtk_widget_set_size_request(register_button, -1, 45);
	g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_clicked), ctx);
	gtk_box_pack_start(GTK_BOX(button_box), register_button, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(form_box), button_box, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
	gtk_widget_set_margin_top(status_label, 10);
	gtk_box_pack_start(GTK_BOX(form_box), status_label, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(form_frame), form_box);
	gtk_box_pack_start(GTK_BOX(content), form_frame, FALSE, FALSE, 0);

	GtkWidget *conn_status = gtk_label_new(NULL);
	char conn_text[128];
	if (ctx->connected) {
		snprintf(conn_text, sizeof(conn_text), "<span size='small' foreground='#27ae60'>Connected to %s:%d</span>", SERVER_IP, SERVER_PORT);
	} else {
		snprintf(conn_text, sizeof(conn_text), "<span size='small' foreground='#95a5a6'>Not connected</span>");
	}
	gtk_label_set_markup(GTK_LABEL(conn_status), conn_text);
	gtk_widget_set_margin_top(conn_status, 15);
	gtk_box_pack_start(GTK_BOX(content), conn_status, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

	GtkWidget *footer = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(footer), "<span size='small' foreground='#95a5a6'>Admin: tkadmin/mkadmin | Participant: register new account</span>");
	gtk_widget_set_margin_bottom(footer, 15);
	gtk_box_pack_end(GTK_BOX(main_box), footer, FALSE, FALSE, 0);

	return main_box;
}

void page_login_update(ClientContext* ctx) {
	(void)ctx;
}
