#include "ui.h"
#include "net.h"
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

static void on_back_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_ADMIN_PANEL;
    ctx->force_page_refresh = true;
}

GtkWidget* create_admin_upload_page(ClientContext* ctx) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(header, 14);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold' foreground='#1c2430'>Upload Question Bank</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subtitle), "<span size='large' foreground='#4f5966'>CSV format: id,question,A,B,C,D,answer</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "header-subtitle");
    gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

    GtkWidget *status_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(status_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_frame), "card");
    status_label = gtk_label_new(ctx->status_message);
    gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-bar");
    gtk_container_add(GTK_CONTAINER(status_frame), status_label);
    gtk_box_pack_start(GTK_BOX(main_box), status_frame, FALSE, FALSE, 0);

    GtkWidget *upload_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(upload_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(upload_frame), "card");
    GtkWidget *upload_label_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(upload_label_header), "<b>[FILE] Upload Question Bank</b>");
    gtk_frame_set_label_widget(GTK_FRAME(upload_frame), upload_label_header);

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
    gtk_style_context_add_class(gtk_widget_get_style_context(filename_entry), "entry");
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
    gtk_widget_set_size_request(scrolled_window, -1, 280);
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
    gtk_widget_set_size_request(upload_button, -1, 44);
    g_signal_connect(upload_button, "clicked", G_CALLBACK(on_upload_csv_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(upload_button), "btn-primary");
    gtk_box_pack_start(GTK_BOX(upload_box), upload_button, FALSE, FALSE, 5);

    gtk_container_add(GTK_CONTAINER(upload_frame), upload_box);
    gtk_box_pack_start(GTK_BOX(main_box), upload_frame, TRUE, TRUE, 0);

    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *back_btn = gtk_button_new_with_label("<< Back to Admin Panel");
    gtk_style_context_add_class(gtk_widget_get_style_context(back_btn), "btn-ghost");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(footer_box), back_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), footer_box, FALSE, FALSE, 0);

    return main_box;
}

void page_admin_upload_update(ClientContext* ctx) {
    if (status_label && ctx->status_message[0] != '\0') {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}
