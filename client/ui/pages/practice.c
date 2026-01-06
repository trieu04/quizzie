#include "ui.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

// UI component pointers
static GtkWidget *status_label = NULL;
static GtkWidget *status_frame = NULL;
static GtkWidget *subject_combo = NULL;
static GtkWidget *easy_spin = NULL;
static GtkWidget *medium_spin = NULL;
static GtkWidget *hard_spin = NULL;
static GtkWidget *duration_spin = NULL;

static const char* get_selected_subject() {
    if (!subject_combo) return "network";
    const char* label = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(subject_combo));
    if (!label) return "network";
    
    // Extract subject name from "subject (E:x M:y H:z)" format
    static char subject[64];
    const char* space = strchr(label, ' ');
    if (space) {
        size_t len = space - label;
        if (len > 63) len = 63;
        strncpy(subject, label, len);
        subject[len] = '\0';
        return subject;
    }
    
    return label;
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
    
    // Check available questions for this subject
    for (int i = 0; i < ctx->practice_subjects_count; i++) {
        if (strcmp(ctx->practice_subjects_list[i].name, subject) == 0) {
            if (easy_req > ctx->practice_subjects_list[i].easy_count ||
                med_req > ctx->practice_subjects_list[i].medium_count ||
                hard_req > ctx->practice_subjects_list[i].hard_count) {
                strncpy(ctx->status_message, "Not enough questions of requested difficulty in selected subject.", sizeof(ctx->status_message) - 1);
                ctx->status_message[sizeof(ctx->status_message) - 1] = '\0';
                if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
                return;
            }
            break;
        }
    }

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
    status_frame = NULL;
    subject_combo = NULL;
    easy_spin = NULL;
    medium_spin = NULL;
    hard_spin = NULL;
    duration_spin = NULL;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");

    // Top Toolbar
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(toolbar, 10);
    gtk_widget_set_margin_bottom(toolbar, 10);

    // Title Section in Toolbar
    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *title = gtk_label_new("Practice Mode");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title"); // Use Consistent Header Class
    gtk_box_pack_start(GTK_BOX(title_box), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new("Practice unlimited with instant feedback");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "page-subtitle");
    gtk_box_pack_start(GTK_BOX(title_box), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(toolbar), title_box, TRUE, TRUE, 0);

    // Back Button in Toolbar
    GtkWidget *btn_back = gtk_button_new_with_label("Back to Dashboard");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_back), "btn-ghost");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_back_clicked), ctx);
    gtk_box_pack_end(GTK_BOX(toolbar), btn_back, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), toolbar, FALSE, FALSE, 0);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(content, 10); // Reduced margin as page has padding
    gtk_widget_set_margin_end(content, 10);
    gtk_widget_set_margin_bottom(content, 30);

    status_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(status_frame), GTK_SHADOW_NONE);
    // gtk_style_context_add_class(gtk_widget_get_style_context(status_frame), "card"); // Removed card class for status to make it cleaner
    status_label = gtk_label_new(ctx->status_message);
    gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
    // gtk_widget_set_margin_start(status_label, 10);
    // gtk_widget_set_margin_end(status_label, 10);
    gtk_widget_set_margin_top(status_label, 8);
    gtk_widget_set_margin_bottom(status_label, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-bar");
    gtk_container_add(GTK_CONTAINER(status_frame), status_label);
    gtk_box_pack_start(GTK_BOX(content), status_frame, FALSE, FALSE, 0);
    // Hide status frame if no message
    if (strlen(ctx->status_message) == 0) {
        gtk_widget_set_no_show_all(status_frame, TRUE);
        gtk_widget_hide(status_frame);
    }

    GtkWidget *card = gtk_frame_new(NULL);
    GtkWidget *card_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(card_header), "<b>Select Practice Bank</b>");
    gtk_frame_set_label_widget(GTK_FRAME(card), card_header);
    gtk_frame_set_shadow_type(GTK_FRAME(card), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "card");

    GtkWidget *card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16); // Increased spacing
    gtk_widget_set_margin_start(card_box, 15);
    gtk_widget_set_margin_end(card_box, 15);
    gtk_widget_set_margin_top(card_box, 15);
    gtk_widget_set_margin_bottom(card_box, 15);

    // Subject selection
    GtkWidget *subject_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *subject_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subject_label), "<b>Subject:</b>");
    gtk_widget_set_size_request(subject_label, 140, -1);
    gtk_widget_set_halign(subject_label, GTK_ALIGN_START); // Align label start
    
    subject_combo = gtk_combo_box_text_new();
    
    // Request practice subjects from server
    client_send_message(ctx, "GET_PRACTICE_SUBJECTS", "");
    ctx->subjects_refreshed = false;
    
    // Populate with available subjects (will be updated when server responds)
    if (ctx->practice_subjects_count > 0) {
        for (int i = 0; i < ctx->practice_subjects_count; i++) {
            char label[128];
            snprintf(label, sizeof(label), "%s (E:%d M:%d H:%d)", 
                    ctx->practice_subjects_list[i].name,
                    ctx->practice_subjects_list[i].easy_count,
                    ctx->practice_subjects_list[i].medium_count,
                    ctx->practice_subjects_list[i].hard_count);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), label);
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(subject_combo), 0);
    } else {
        // Fallback defaults while loading
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "network");
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "programming");
        gtk_combo_box_set_active(GTK_COMBO_BOX(subject_combo), 0);
    }
    
    gtk_box_pack_start(GTK_BOX(subject_row), subject_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(subject_row), subject_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card_box), subject_row, FALSE, FALSE, 0);

    // Difficulty row
    GtkWidget *diff_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *diff_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(diff_label), "<b>Questions:</b>");
    gtk_widget_set_size_request(diff_label, 140, -1);
    gtk_widget_set_halign(diff_label, GTK_ALIGN_START);

    GtkWidget *diff_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    
    // Easy
    GtkWidget *easy_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *easy_label = gtk_label_new("Easy");
    easy_spin = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(easy_spin), 3);
    gtk_box_pack_start(GTK_BOX(easy_vbox), easy_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(easy_vbox), easy_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_box), easy_vbox, FALSE, FALSE, 0);

    // Medium
    GtkWidget *med_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *medium_label = gtk_label_new("Medium");
    medium_spin = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(medium_spin), 4);
    gtk_box_pack_start(GTK_BOX(med_vbox), medium_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(med_vbox), medium_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_box), med_vbox, FALSE, FALSE, 0);

    // Hard
    GtkWidget *hard_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *hard_label = gtk_label_new("Hard");
    hard_spin = gtk_spin_button_new_with_range(0, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(hard_spin), 3);
    gtk_box_pack_start(GTK_BOX(hard_vbox), hard_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hard_vbox), hard_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_box), hard_vbox, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(diff_row), diff_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diff_row), diff_box, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card_box), diff_row, FALSE, FALSE, 0);

    // Duration row
    GtkWidget *dur_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *dur_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(dur_label), "<b>Duration (sec):</b>");
    gtk_widget_set_size_request(dur_label, 140, -1);
    gtk_widget_set_halign(dur_label, GTK_ALIGN_START);
    
    duration_spin = gtk_spin_button_new_with_range(60, 1800, 60);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(duration_spin), 600);
    gtk_widget_set_halign(duration_spin, GTK_ALIGN_START);
    
    gtk_box_pack_start(GTK_BOX(dur_row), dur_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dur_row), duration_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card_box), dur_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(card), card_box);
    gtk_box_pack_start(GTK_BOX(content), card, FALSE, FALSE, 0);

    // Primary Action Button (Start)
    GtkWidget *action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(action_box, GTK_ALIGN_END); // Align to end for action feeling

    GtkWidget *btn_start = gtk_button_new_with_label("Start Practice Session");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_start), "btn-primary");
    gtk_widget_set_size_request(btn_start, 200, 45); // Larger clickable area
    g_signal_connect(btn_start, "clicked", G_CALLBACK(on_start_practice), ctx);

    gtk_box_pack_start(GTK_BOX(action_box), btn_start, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content), action_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);

    return main_box;
}

void page_practice_update(ClientContext* ctx) {
    // Update status label with any server messages
    if (status_label && ctx->status_message[0] != '\0') {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        if (status_frame) {
            gtk_widget_set_no_show_all(status_frame, FALSE);
            gtk_widget_show_all(status_frame);
        }
        // Don't clear the message here - let it persist until user takes action
    } else if (status_frame && ctx->status_message[0] == '\0') {
        gtk_widget_hide(status_frame);
    }    
    // Update subject combo if subjects were refreshed
    if (subject_combo && ctx->subjects_refreshed && ctx->practice_subjects_count > 0) {
        // Clear existing items
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(subject_combo));
        
        // Add new subjects with stats
        for (int i = 0; i < ctx->practice_subjects_count; i++) {
            char label[128];
            snprintf(label, sizeof(label), "%s (E:%d M:%d H:%d)", 
                    ctx->practice_subjects_list[i].name,
                    ctx->practice_subjects_list[i].easy_count,
                    ctx->practice_subjects_list[i].medium_count,
                    ctx->practice_subjects_list[i].hard_count);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), label);
        }
        
        // Select first item
        gtk_combo_box_set_active(GTK_COMBO_BOX(subject_combo), 0);
        
        ctx->subjects_refreshed = false;
    }}
