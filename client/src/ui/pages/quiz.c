#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

static GtkWidget *status_label = NULL;
static GtkWidget *timer_label = NULL;
static GtkWidget *question_label = NULL;
static GtkWidget *radio_buttons[4] = {NULL};
static GtkWidget *question_list_box = NULL;
static GtkWidget *submit_btn = NULL;
static GtkWidget *start_btn = NULL;
static GtkWidget *waiting_label = NULL;
static GSList *radio_group = NULL;
static guint timer_id = 0;

static gboolean update_timer(gpointer data) {
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->quiz_start_time > 0 && ctx->quiz_duration > 0 && timer_label) {
        time_t now = time(NULL);
        int elapsed = (int)(now - ctx->quiz_start_time);
        int remaining = ctx->quiz_duration - elapsed;
        if (remaining < 0) remaining = 0;
        
        int mins = remaining / 60;
        int secs = remaining % 60;
        
        char time_str[32];
        snprintf(time_str, sizeof(time_str), "Time: %02d:%02d", mins, secs);
        gtk_label_set_text(GTK_LABEL(timer_label), time_str);
        
        // Auto-submit when time is up
        if (remaining == 0 && ctx->question_count > 0) {
            char answer_str[MAX_QUESTIONS + 1];
            for (int i = 0; i < ctx->question_count; i++) {
                answer_str[i] = ctx->answers[i] ? ctx->answers[i] : '-';
            }
            answer_str[ctx->question_count] = '\0';
            client_send_message(ctx, "SUBMIT", answer_str);
            strcpy(ctx->status_message, "Time's up! Auto-submitting...");
            return G_SOURCE_REMOVE;
        }
    }
    
    return G_SOURCE_CONTINUE;
}

static void update_question_display(ClientContext* ctx) {
    if (!question_label || ctx->question_count == 0) return;
    
    Question* q = &ctx->questions[ctx->current_question];
    
    char q_text[512];
    snprintf(q_text, sizeof(q_text), "Question %d: %s", 
             ctx->current_question + 1, q->question);
    gtk_label_set_text(GTK_LABEL(question_label), q_text);
    
    // Update radio buttons
    char cur_answer = ctx->answers[ctx->current_question];
    for (int i = 0; i < 4; i++) {
        if (radio_buttons[i]) {
            char label[256];
            char opt = 'A' + i;
            snprintf(label, sizeof(label), "%c. %s", opt, q->options[i]);
            gtk_button_set_label(GTK_BUTTON(radio_buttons[i]), label);
            
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_buttons[i]), 
                                        cur_answer == opt);
        }
    }
}

static void on_question_selected(GtkWidget *widget, gpointer data) {
    int q_num = GPOINTER_TO_INT(data);
    ClientContext* ctx = ui_get_context()->client_ctx;
    
    (void)widget;
    if (ctx) {
        ctx->current_question = q_num;
        update_question_display(ctx);
    }
}

static void on_answer_selected(GtkToggleButton *button, gpointer data) {
    if (!gtk_toggle_button_get_active(button)) return;
    
    char answer = GPOINTER_TO_INT(data);
    ClientContext* ctx = ui_get_context()->client_ctx;
    
    if (ctx && ctx->question_count > 0) {
        ctx->answers[ctx->current_question] = answer;
        
        // Update question list to show answer
        if (question_list_box) {
            GList *children = gtk_container_get_children(GTK_CONTAINER(question_list_box));
            if (g_list_nth(children, ctx->current_question)) {
                GtkWidget *btn = GTK_WIDGET(g_list_nth_data(children, ctx->current_question));
                char label[32];
                snprintf(label, sizeof(label), "Q%d [%c]", ctx->current_question + 1, answer);
                gtk_button_set_label(GTK_BUTTON(btn), label);
            }
            g_list_free(children);
        }
    }
}

static void on_submit_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected && ctx->question_count > 0) {
        char answer_str[MAX_QUESTIONS + 1];
        for (int i = 0; i < ctx->question_count; i++) {
            answer_str[i] = ctx->answers[i] ? ctx->answers[i] : '-';
        }
        answer_str[ctx->question_count] = '\0';
        
        client_send_message(ctx, "SUBMIT", answer_str);
        strcpy(ctx->status_message, "Answers submitted!");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    }
}

static void on_start_quiz_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected && ctx->quiz_available) {
        client_send_message(ctx, "CLIENT_START", "");
        strcpy(ctx->status_message, "Starting your quiz...");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    }
}

static void on_leave_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    ctx->current_room_id = -1;
    ctx->question_count = 0;
    ctx->current_question = 0;
    ctx->is_host = false;
    ctx->quiz_available = false;
    ctx->quiz_start_time = 0;
    memset(ctx->answers, 0, sizeof(ctx->answers));
    ctx->current_state = PAGE_DASHBOARD;
    
    if (timer_id > 0) {
        g_source_remove(timer_id);
        timer_id = 0;
    }
    
    ui_navigate_to_page(PAGE_DASHBOARD);
}

GtkWidget* page_quiz_create(ClientContext* ctx) {
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    // Sidebar
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(sidebar, 200, -1);
    gtk_widget_set_margin_start(sidebar, 10);
    gtk_widget_set_margin_end(sidebar, 10);
    gtk_widget_set_margin_top(sidebar, 10);
    gtk_widget_set_margin_bottom(sidebar, 10);
    
    GtkWidget *sidebar_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sidebar_title), "<b>QUIZ</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), sidebar_title, FALSE, FALSE, 0);
    
    char room_text[64];
    snprintf(room_text, sizeof(room_text), "Room: %d", ctx->current_room_id);
    GtkWidget *room_label = gtk_label_new(room_text);
    gtk_box_pack_start(GTK_BOX(sidebar), room_label, FALSE, FALSE, 0);
    
    char player_text[64];
    snprintf(player_text, sizeof(player_text), "Player: %s", ctx->username);
    GtkWidget *player_label = gtk_label_new(player_text);
    gtk_box_pack_start(GTK_BOX(sidebar), player_label, FALSE, FALSE, 0);
    
    timer_label = gtk_label_new("Time: --:--");
    gtk_box_pack_start(GTK_BOX(sidebar), timer_label, FALSE, FALSE, 5);
    
    char progress_text[64];
    snprintf(progress_text, sizeof(progress_text), "Progress: %d / %d", 
             ctx->current_question + 1, ctx->question_count > 0 ? ctx->question_count : 1);
    GtkWidget *progress_label = gtk_label_new(progress_text);
    gtk_box_pack_start(GTK_BOX(sidebar), progress_label, FALSE, FALSE, 0);
    
    // Question list
    GtkWidget *q_label = gtk_label_new("Questions:");
    gtk_box_pack_start(GTK_BOX(sidebar), q_label, FALSE, FALSE, 5);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    question_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(scrolled), question_list_box);
    gtk_box_pack_start(GTK_BOX(sidebar), scrolled, TRUE, TRUE, 0);
    
    GtkWidget *leave_btn = gtk_button_new_with_label("Leave Quiz");
    g_signal_connect(leave_btn, "clicked", G_CALLBACK(on_leave_clicked), ctx);
    gtk_box_pack_end(GTK_BOX(sidebar), leave_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(page), sidebar, FALSE, FALSE, 0);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(page), separator, FALSE, FALSE, 0);
    
    // Main content
    GtkWidget *main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(main_area, 20);
    gtk_widget_set_margin_end(main_area, 20);
    gtk_widget_set_margin_top(main_area, 20);
    gtk_widget_set_margin_bottom(main_area, 20);
    
    status_label = gtk_label_new(ctx->status_message);
    gtk_box_pack_start(GTK_BOX(main_area), status_label, FALSE, FALSE, 0);
    
    if (ctx->question_count == 0) {
        if (ctx->quiz_available) {
            waiting_label = gtk_label_new(NULL);
            char markup[256];
            snprintf(markup, sizeof(markup), 
                    "<b>Quiz is ready!</b>\n\nTime limit: %d seconds", ctx->quiz_duration);
            gtk_label_set_markup(GTK_LABEL(waiting_label), markup);
            gtk_box_pack_start(GTK_BOX(main_area), waiting_label, TRUE, FALSE, 0);
            
            start_btn = gtk_button_new_with_label("Start Quiz");
            gtk_widget_set_size_request(start_btn, 150, 40);
            g_signal_connect(start_btn, "clicked", G_CALLBACK(on_start_quiz_clicked), ctx);
            gtk_box_pack_start(GTK_BOX(main_area), start_btn, FALSE, FALSE, 0);
        } else {
            waiting_label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(waiting_label), 
                               "<b>Waiting for quiz to start...</b>\n\nWaiting for host to start the game...");
            gtk_box_pack_start(GTK_BOX(main_area), waiting_label, TRUE, FALSE, 0);
        }
    } else {
        // Display question
        question_label = gtk_label_new("");
        gtk_label_set_line_wrap(GTK_LABEL(question_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(question_label), 80);
        gtk_widget_set_halign(question_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(main_area), question_label, FALSE, FALSE, 10);
        
        // Answer options
        radio_group = NULL;
        for (int i = 0; i < 4; i++) {
            radio_buttons[i] = gtk_radio_button_new_with_label(radio_group, "");
            radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_buttons[i]));
            g_signal_connect(radio_buttons[i], "toggled", 
                           G_CALLBACK(on_answer_selected), GINT_TO_POINTER('A' + i));
            gtk_box_pack_start(GTK_BOX(main_area), radio_buttons[i], FALSE, FALSE, 5);
        }
        
        submit_btn = gtk_button_new_with_label("Submit Answers");
        gtk_widget_set_size_request(submit_btn, 150, 40);
        g_signal_connect(submit_btn, "clicked", G_CALLBACK(on_submit_clicked), ctx);
        gtk_box_pack_end(GTK_BOX(main_area), submit_btn, FALSE, FALSE, 10);
        
        // Populate question list
        for (int i = 0; i < ctx->question_count; i++) {
            char btn_label[32];
            char answered = ctx->answers[i] != 0 ? ctx->answers[i] : '-';
            snprintf(btn_label, sizeof(btn_label), "Q%d [%c]", i + 1, answered);
            
            GtkWidget *q_btn = gtk_button_new_with_label(btn_label);
            g_signal_connect(q_btn, "clicked", G_CALLBACK(on_question_selected), GINT_TO_POINTER(i));
            gtk_box_pack_start(GTK_BOX(question_list_box), q_btn, FALSE, FALSE, 0);
        }
        
        update_question_display(ctx);
        
        // Start timer
        if (ctx->quiz_start_time > 0) {
            timer_id = g_timeout_add(1000, update_timer, ctx);
        }
    }
    
    gtk_box_pack_start(GTK_BOX(page), main_area, TRUE, TRUE, 0);
    
    return page;
}

void page_quiz_update(ClientContext* ctx) {
    if (ctx->question_count > 0 && question_label) {
        update_question_display(ctx);
    }
    
    if (status_label && strlen(ctx->status_message) > 0) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}

void page_quiz_cleanup() {
    if (timer_id > 0) {
        g_source_remove(timer_id);
        timer_id = 0;
    }
}
