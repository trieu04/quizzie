#include "ui.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

// UI component pointers
static GtkWidget *status_label = NULL;
static GtkWidget *subject_combo = NULL;
static GtkWidget *easy_spin = NULL;
static GtkWidget *medium_spin = NULL;
static GtkWidget *hard_spin = NULL;
static GtkWidget *duration_spin = NULL;

static const char* get_selected_subject() {
    if (!subject_combo) return "math";
    const char* subj = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(subject_combo));
    return subj ? subj : "math";
}

static void on_back_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    if (!ctx) return;
    ctx->current_state = PAGE_DASHBOARD;
    ui_navigate_to_page(PAGE_DASHBOARD);
}

static void on_start_practice(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    if (!ctx) return;

    int easy_req = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(easy_spin));
    int med_req  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(medium_spin));
    int hard_req = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(hard_spin));
    int requested = easy_req + med_req + hard_req;
    if (requested <= 0) {
        strncpy(ctx->status_message, "Choose at least 1 question.", sizeof(ctx->status_message) - 1);
        ctx->status_message[sizeof(ctx->status_message) - 1] = '\0';
        if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        return;
    }
    if (requested > MAX_QUESTIONS) {
        requested = MAX_QUESTIONS;
    }

    const char* subject = get_selected_subject();
    strncpy(ctx->subject, subject, sizeof(ctx->subject) - 1);
    ctx->subject[sizeof(ctx->subject) - 1] = '\0';

    // Get duration from spinner
    int duration = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(duration_spin));
    ctx->quiz_duration = duration;

    // Send LOAD_PRACTICE_QUESTIONS to server: subject,easy_count,medium_count,hard_count
    char request[128];
    snprintf(request, sizeof(request), "%s,%d,%d,%d", subject, easy_req, med_req, hard_req);
    client_send_message(ctx, "LOAD_PRACTICE_QUESTIONS", request);

    // Update UI
    strncpy(ctx->status_message, "Loading practice questions from server...", sizeof(ctx->status_message) - 1);
    ctx->status_message[sizeof(ctx->status_message) - 1] = '\0';
    if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
}

GtkWidget* page_practice_create(ClientContext* ctx) {
    status_label = NULL;
    subject_combo = NULL;
    easy_spin = NULL;
    medium_spin = NULL;
    hard_spin = NULL;
    duration_spin = NULL;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(header, 20);
    gtk_widget_set_margin_bottom(header, 20);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold' foreground='#1c2430'>Practice Mode</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 5);

    GtkWidget *subtitle = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subtitle), "<span size='large' foreground='#27ae60'>Server-delivered practice questions for all difficulty levels.</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "header-subtitle");
    gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(content, 30);
    gtk_widget_set_margin_end(content, 30);
    gtk_widget_set_margin_bottom(content, 30);

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

    GtkWidget *card = gtk_frame_new(NULL);
    GtkWidget *card_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(card_header), "<b>Select Practice Bank</b>");
    gtk_frame_set_label_widget(GTK_FRAME(card), card_header);
    gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "card");

    GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(card_box, 15);
    gtk_widget_set_margin_end(card_box, 15);
    gtk_widget_set_margin_top(card_box, 15);
    gtk_widget_set_margin_bottom(card_box, 15);

    // Subject selection
    GtkWidget *subject_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *subject_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subject_label), "<b>Subject:</b>");
    gtk_widget_set_size_request(subject_label, 120, -1);
    subject_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "math");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "network");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "programming");
    gtk_combo_box_set_active(GTK_COMBO_BOX(subject_combo), 0);
    gtk_box_pack_start(GTK_BOX(subject_row), subject_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(subject_row), subject_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card_box), subject_row, FALSE, FALSE, 0);

    // Difficulty row
    GtkWidget *diff_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *diff_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(diff_label), "<b>Questions:</b>");
    gtk_widget_set_size_request(diff_label, 120, -1);

    GtkWidget *diff_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    
    // Easy
    GtkWidget *easy_label = gtk_label_new("Easy:");
    easy_spin = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(easy_spin), 3);
    gtk_box_pack_start(GTK_BOX(diff_box), easy_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_box), easy_spin, FALSE, FALSE, 0);

    // Medium
    GtkWidget *medium_label = gtk_label_new("Medium:");
    medium_spin = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(medium_spin), 4);
    gtk_box_pack_start(GTK_BOX(diff_box), medium_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_box), medium_spin, FALSE, FALSE, 0);

    // Hard
    GtkWidget *hard_label = gtk_label_new("Hard:");
    hard_spin = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(hard_spin), 3);
    gtk_box_pack_start(GTK_BOX(diff_box), hard_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_box), hard_spin, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(diff_row), diff_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_row), diff_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card_box), diff_row, FALSE, FALSE, 0);

    // Duration row
    GtkWidget *dur_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *dur_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(dur_label), "<b>Duration (seconds):</b>");
    gtk_widget_set_size_request(dur_label, 120, -1);
    duration_spin = gtk_spin_button_new_with_range(60, 1800, 60);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(duration_spin), 600);
    gtk_box_pack_start(GTK_BOX(dur_row), dur_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dur_row), duration_spin, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card_box), dur_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(card), card_box);
    gtk_box_pack_start(GTK_BOX(content), card, FALSE, FALSE, 0);

    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);

    GtkWidget *btn_start = gtk_button_new_with_label("Start Practice");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_start), "btn-primary");
    g_signal_connect(btn_start, "clicked", G_CALLBACK(on_start_practice), ctx);

    GtkWidget *btn_back = gtk_button_new_with_label("Back");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_back), "btn-secondary");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_back_clicked), ctx);

    gtk_box_pack_start(GTK_BOX(button_box), btn_start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(button_box), btn_back, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), button_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

    return main_box;
}

void page_practice_update(ClientContext* ctx) {
    (void)ctx;
    // Update logic here if needed
}
