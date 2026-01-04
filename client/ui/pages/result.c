#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>

static void on_play_again_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    // Reset quiz state
    ctx->score = 0;
    ctx->question_count = 0;
    ctx->current_question = 0;
    ctx->quiz_start_time = 0;
    ctx->time_taken = 0;
    ctx->quiz_available = false;
    ctx->is_practice = false;
    memset(ctx->answers, 0, sizeof(ctx->answers));
    ctx->status_message[0] = '\0';
    ctx->current_room_id = -1;
    ctx->current_state = PAGE_ROOM_LIST;
    
    ui_navigate_to_page(PAGE_ROOM_LIST);
}

static void on_dashboard_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    // Reset quiz state
    ctx->score = 0;
    ctx->question_count = 0;
    ctx->current_question = 0;
    ctx->quiz_start_time = 0;
    ctx->time_taken = 0;
    ctx->quiz_available = false;
    ctx->is_practice = false;
    memset(ctx->answers, 0, sizeof(ctx->answers));
    ctx->status_message[0] = '\0';
    ctx->current_room_id = -1;
    ctx->is_host = false;
    ctx->current_state = PAGE_DASHBOARD;
    
    ui_navigate_to_page(PAGE_DASHBOARD);
}

GtkWidget* page_result_create(ClientContext* ctx) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_style_context_add_class(gtk_widget_get_style_context(page), "page");
    gtk_widget_set_halign(page, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(page, GTK_ALIGN_CENTER);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "card");
    gtk_box_pack_start(GTK_BOX(page), card, FALSE, FALSE, 0);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>QUIZ RESULTS</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(card), title, FALSE, FALSE, 0);

    GtkWidget *score_title = gtk_label_new("Your Score:");
    gtk_box_pack_start(GTK_BOX(card), score_title, FALSE, FALSE, 0);

    char score_str[64];
    snprintf(score_str, sizeof(score_str), "%d / %d", ctx->score, ctx->total_questions);
    GtkWidget *score_label = gtk_label_new(NULL);
    char score_markup[128];
    snprintf(score_markup, sizeof(score_markup), 
             "<span size='xx-large' weight='bold'>%s</span>", score_str);
    gtk_label_set_markup(GTK_LABEL(score_label), score_markup);
    gtk_box_pack_start(GTK_BOX(card), score_label, FALSE, FALSE, 0);

    float percentage = ctx->total_questions > 0 ? 
                       (float)ctx->score / ctx->total_questions * 100 : 0;
    char percent_str[64];
    snprintf(percent_str, sizeof(percent_str), "Percentage: %.1f%%", percentage);
    GtkWidget *percent_label = gtk_label_new(percent_str);
    gtk_style_context_add_class(gtk_widget_get_style_context(percent_label), "progress-label");
    gtk_box_pack_start(GTK_BOX(card), percent_label, FALSE, FALSE, 0);

    GtkWidget *level = gtk_level_bar_new_for_interval(0.0, 100.0);
    gtk_level_bar_set_value(GTK_LEVEL_BAR(level), percentage);
    gtk_widget_set_margin_bottom(level, 6);
    gtk_box_pack_start(GTK_BOX(card), level, FALSE, FALSE, 0);

    if (ctx->time_taken > 0) {
        int mins = ctx->time_taken / 60;
        int secs = ctx->time_taken % 60;
        char time_str[64];
        snprintf(time_str, sizeof(time_str), "Time taken: %02d:%02d", mins, secs);
        GtkWidget *time_label = gtk_label_new(time_str);
        gtk_box_pack_start(GTK_BOX(card), time_label, FALSE, FALSE, 0);
    }

    const char* message;
    if (percentage >= 80) {
        message = "Excellent! Great job!";
    } else if (percentage >= 60) {
        message = "Good work! Keep it up!";
    } else if (percentage >= 40) {
        message = "Not bad! Room for improvement.";
    } else {
        message = "Keep practicing!";
    }
    GtkWidget *message_label = gtk_label_new(message);
    gtk_style_context_add_class(gtk_widget_get_style_context(message_label), "status-bar");
    gtk_box_pack_start(GTK_BOX(card), message_label, FALSE, FALSE, 5);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_size_request(button_box, 260, -1);

    GtkWidget *play_again_btn = gtk_button_new_with_label("Play Again");
    gtk_widget_set_size_request(play_again_btn, -1, 45);
    g_signal_connect(play_again_btn, "clicked", G_CALLBACK(on_play_again_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(play_again_btn), "btn-primary");
    gtk_box_pack_start(GTK_BOX(button_box), play_again_btn, TRUE, TRUE, 0);

    GtkWidget *dashboard_btn = gtk_button_new_with_label("Dashboard");
    gtk_widget_set_size_request(dashboard_btn, -1, 45);
    g_signal_connect(dashboard_btn, "clicked", G_CALLBACK(on_dashboard_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(dashboard_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(button_box), dashboard_btn, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(card), button_box, FALSE, FALSE, 4);

    return page;
}

void page_result_update(ClientContext* ctx) {
    // This function is called when still on result page
    // Navigation is handled by ui.c
    (void)ctx;
}
