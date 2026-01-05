
#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *server_entry = NULL;
static GtkWidget *port_entry = NULL;
static GtkWidget *username_entry = NULL;
static GtkWidget *password_entry = NULL;
static GtkWidget *confirm_password_entry = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *register_button = NULL;
static GtkWidget *login_link = NULL;

static void on_login_link_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;
	ctx->current_state = PAGE_LOGIN;
	ui_navigate_to_page(PAGE_LOGIN);
}

static void on_register_clicked(GtkWidget *widget, gpointer data) {
	(void)widget;
	ClientContext* ctx = (ClientContext*)data;

	const char* server = gtk_entry_get_text(GTK_ENTRY(server_entry));
	const char* port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
	const char* username = gtk_entry_get_text(GTK_ENTRY(username_entry));
	const char* password = gtk_entry_get_text(GTK_ENTRY(password_entry));
	const char* confirm_password = gtk_entry_get_text(GTK_ENTRY(confirm_password_entry));

	if (strlen(server) == 0 || strlen(port_str) == 0) {
		gtk_label_set_text(GTK_LABEL(status_label), "Server and port cannot be empty!");
		gtk_widget_show(status_label);
		return;
	}

	if (strlen(username) == 0 || strlen(password) == 0) {
		gtk_label_set_text(GTK_LABEL(status_label), "Username and password cannot be empty!");
		gtk_widget_show(status_label);
		return;
	}

	if (strlen(username) < 3) {
		gtk_label_set_text(GTK_LABEL(status_label), "Username must be at least 3 characters!");
		gtk_widget_show(status_label);
		return;
	}

	if (strlen(password) < 6) {
		gtk_label_set_text(GTK_LABEL(status_label), "Password must be at least 6 characters!");
		gtk_widget_show(status_label);
		return;
	}

	if (strcmp(password, confirm_password) != 0) {
		gtk_label_set_text(GTK_LABEL(status_label), "Passwords do not match!");
		gtk_widget_show(status_label);
		return;
	}

	int port = atoi(port_str);
	if (port <= 0 || port > 65535) {
		gtk_label_set_text(GTK_LABEL(status_label), "Invalid port number!");
		gtk_widget_show(status_label);
		return;
	}

	// Store server connection info
	strncpy(ctx->server_ip, server, sizeof(ctx->server_ip) - 1);
	ctx->server_ip[sizeof(ctx->server_ip) - 1] = '\0';
	ctx->server_port = port;

	if (!ctx->connected) {
		gtk_label_set_text(GTK_LABEL(status_label), "Connecting to server...");
		gtk_widget_show(status_label);
		if (net_connect(ctx, ctx->server_ip, ctx->server_port) != 0) {
			gtk_label_set_text(GTK_LABEL(status_label), "Failed to connect to server!");
			gtk_widget_show(status_label);
			return;
		}
	}

	char msg[128];
	snprintf(msg, sizeof(msg), "%s,%s", username, password);
	client_send_message(ctx, "REGISTER", msg);
	gtk_label_set_text(GTK_LABEL(status_label), "Registering...");
	gtk_widget_show(status_label);
}

static gboolean on_entry_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	(void)widget;
	if (event->keyval == GDK_KEY_Return) {
		on_register_clicked(NULL, data);
		return TRUE;
	}
	return FALSE;
}

GtkWidget* page_register_create(ClientContext* ctx) {
	// Reset statics to avoid stale widget pointers between navigations
	server_entry = NULL;
	port_entry = NULL;
	username_entry = NULL;
	password_entry = NULL;
	confirm_password_entry = NULL;
	status_label = NULL;
	register_button = NULL;
	login_link = NULL;

	GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkStyleContext *main_ctx = gtk_widget_get_style_context(main_box);
	gtk_style_context_add_class(main_ctx, "page");

	GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_top(header, 25);
	gtk_widget_set_margin_bottom(header, 20);

	GtkWidget *title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(title), "<span size='30000' weight='bold' foreground='#1c2430'>Create Account</span>");
	gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
	gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

	GtkWidget *subtitle = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(subtitle), "<span size='large' foreground='#5b6472'>Join the Quiz Community</span>");
	gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "header-subtitle");
	gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
	gtk_widget_set_halign(content, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(content, GTK_ALIGN_CENTER);

	GtkWidget *form_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(form_frame), GTK_SHADOW_NONE);
	GtkWidget *form_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(form_label), "<b>Registration Form</b>");
	gtk_frame_set_label_widget(GTK_FRAME(form_frame), form_label);
	gtk_style_context_add_class(gtk_widget_get_style_context(form_frame), "card");

	GtkWidget *form_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_size_request(form_box, 400, -1);
	gtk_widget_set_margin_start(form_box, 25);
	gtk_widget_set_margin_end(form_box, 25);
	gtk_widget_set_margin_top(form_box, 15);
	gtk_widget_set_margin_bottom(form_box, 15);

	// Server connection section
	GtkWidget *server_section = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(server_section), "<b>Server Connection</b>");
	gtk_widget_set_halign(server_section, GTK_ALIGN_START);
	gtk_widget_set_margin_bottom(server_section, 5);
	gtk_box_pack_start(GTK_BOX(form_box), server_section, FALSE, FALSE, 0);

	GtkWidget *server_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

	GtkWidget *server_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	GtkWidget *server_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(server_label), "Server IP/Domain:");
	gtk_widget_set_halign(server_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(server_vbox), server_label, FALSE, FALSE, 0);

	server_entry = gtk_entry_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(server_entry), "entry");
	gtk_entry_set_placeholder_text(GTK_ENTRY(server_entry), "127.0.0.1");
	gtk_entry_set_text(GTK_ENTRY(server_entry), "127.0.0.1");
	gtk_widget_set_size_request(server_entry, -1, 35);
	g_signal_connect(server_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(server_vbox), server_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(server_box), server_vbox, TRUE, TRUE, 0);

	GtkWidget *port_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	GtkWidget *port_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(port_label), "Port:");
	gtk_widget_set_halign(port_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(port_vbox), port_label, FALSE, FALSE, 0);

	port_entry = gtk_entry_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(port_entry), "entry");
	gtk_entry_set_placeholder_text(GTK_ENTRY(port_entry), "8080");
	gtk_entry_set_text(GTK_ENTRY(port_entry), "8080");
	gtk_entry_set_max_length(GTK_ENTRY(port_entry), 5);
	gtk_widget_set_size_request(port_entry, 100, 35);
	g_signal_connect(port_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(port_vbox), port_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(server_box), port_vbox, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(form_box), server_box, FALSE, FALSE, 0);

	// Separator
	GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_margin_top(separator, 10);
	gtk_widget_set_margin_bottom(separator, 10);
	gtk_box_pack_start(GTK_BOX(form_box), separator, FALSE, FALSE, 0);

	// Account details section
	GtkWidget *account_section = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(account_section), "<b>Account Details</b>");
	gtk_widget_set_halign(account_section, GTK_ALIGN_START);
	gtk_widget_set_margin_bottom(account_section, 5);
	gtk_box_pack_start(GTK_BOX(form_box), account_section, FALSE, FALSE, 0);

	GtkWidget *username_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(username_label), "Username:");
	gtk_widget_set_halign(username_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(form_box), username_label, FALSE, FALSE, 0);

	username_entry = gtk_entry_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(username_entry), "entry");
	gtk_entry_set_placeholder_text(GTK_ENTRY(username_entry), "Choose a username (min 3 chars)");
	gtk_entry_set_max_length(GTK_ENTRY(username_entry), 31);
	gtk_widget_set_size_request(username_entry, -1, 35);
	g_signal_connect(username_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(form_box), username_entry, FALSE, FALSE, 0);

	GtkWidget *password_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(password_label), "Password:");
	gtk_widget_set_halign(password_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(form_box), password_label, FALSE, FALSE, 0);

	password_entry = gtk_entry_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(password_entry), "entry");
	gtk_entry_set_placeholder_text(GTK_ENTRY(password_entry), "Choose a password (min 6 chars)");
	gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
	gtk_entry_set_max_length(GTK_ENTRY(password_entry), 31);
	gtk_widget_set_size_request(password_entry, -1, 35);
	g_signal_connect(password_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(form_box), password_entry, FALSE, FALSE, 0);

	GtkWidget *confirm_password_label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(confirm_password_label), "Confirm Password:");
	gtk_widget_set_halign(confirm_password_label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(form_box), confirm_password_label, FALSE, FALSE, 0);

	confirm_password_entry = gtk_entry_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(confirm_password_entry), "entry");
	gtk_entry_set_placeholder_text(GTK_ENTRY(confirm_password_entry), "Re-enter your password");
	gtk_entry_set_visibility(GTK_ENTRY(confirm_password_entry), FALSE);
	gtk_entry_set_max_length(GTK_ENTRY(confirm_password_entry), 31);
	gtk_widget_set_size_request(confirm_password_entry, -1, 35);
	g_signal_connect(confirm_password_entry, "key-press-event", G_CALLBACK(on_entry_key_press), ctx);
	gtk_box_pack_start(GTK_BOX(form_box), confirm_password_entry, FALSE, FALSE, 0);

	register_button = gtk_button_new_with_label("CREATE ACCOUNT");
	gtk_widget_set_size_request(register_button, -1, 45);
	gtk_widget_set_margin_top(register_button, 8);
	g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(register_button), "btn-primary");
	gtk_box_pack_start(GTK_BOX(form_box), register_button, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
	gtk_widget_set_margin_top(status_label, 8);
	gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-bar");
	gtk_box_pack_start(GTK_BOX(form_box), status_label, FALSE, FALSE, 0);
	// Hide initially if empty
	if (strlen(ctx->status_message) == 0) {
		gtk_widget_set_no_show_all(status_label, TRUE);
		gtk_widget_hide(status_label);
	}

	// Login link
	GtkWidget *login_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_set_halign(login_box, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(login_box, 10);
	
	GtkWidget *login_text = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(login_text), "<span foreground='#5b6472'>Already have an account?</span>");
	gtk_box_pack_start(GTK_BOX(login_box), login_text, FALSE, FALSE, 0);

	login_link = gtk_button_new_with_label("Login here");
	g_signal_connect(login_link, "clicked", G_CALLBACK(on_login_link_clicked), ctx);
	gtk_style_context_add_class(gtk_widget_get_style_context(login_link), "btn-link");
	gtk_box_pack_start(GTK_BOX(login_box), login_link, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(form_box), login_box, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(form_frame), form_box);
	gtk_box_pack_start(GTK_BOX(content), form_frame, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

	GtkWidget *footer = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(footer), "<span size='small' foreground='#95a5a6'>© 2026 Quizzie - All Rights Reserved</span>");
	gtk_widget_set_margin_bottom(footer, 15);
	gtk_box_pack_end(GTK_BOX(main_box), footer, FALSE, FALSE, 0);

	return main_box;
}

void page_register_update(ClientContext* ctx) {
	if (status_label && ctx->status_message[0] != '\0') {
		gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
		gtk_widget_set_no_show_all(status_label, FALSE);
		gtk_widget_show(status_label);
		// Clear status message after displaying
		ctx->status_message[0] = '\0';
	} else if (status_label && ctx->status_message[0] == '\0') {
		gtk_widget_hide(status_label);
	}
}
