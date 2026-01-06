#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

static GtkWidget *status_label = NULL;
static GtkWidget *timer_label = NULL;
static GtkWidget *question_label = NULL;
static GtkWidget *radio_buttons[4] = {NULL};
static GtkWidget *question_list_box = NULL;
static GtkWidget *question_buttons[MAX_QUESTIONS] = {NULL};
static GtkWidget *filter_combo = NULL;
static GtkWidget *submit_btn = NULL;
static GtkWidget *start_btn = NULL;
static GtkWidget *waiting_label = NULL;
static GtkWidget *progress_label = NULL;
static GSList *radio_group = NULL;
static guint timer_id = 0;
static char difficulty_filter[16] = "all";

static bool difficulty_matches(const char* difficulty) {
    if (!difficulty || strlen(difficulty) == 0) return true;
    if (strcmp(difficulty_filter, "all") == 0) return true;
    char lower[16];
    strncpy(lower, difficulty, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (char* p = lower; *p; ++p) *p = (char)tolower(*p);
    return strncmp(lower, difficulty_filter, sizeof(lower)) == 0;
}

static void rebuild_question_list(ClientContext* ctx);
static void update_question_button_label(ClientContext* ctx, int idx);
static void on_filter_changed(GtkComboBoxText *combo, gpointer data);
static void on_question_selected(GtkWidget *widget, gpointer data);

static void update_question_button_label(ClientContext* ctx, int idx) {
    if (!ctx || idx < 0 || idx >= ctx->question_count) return;
    char diff[16];
    strncpy(diff, ctx->questions[idx].difficulty, sizeof(diff) - 1);
    diff[sizeof(diff) - 1] = '\0';
    if (diff[0]) diff[0] = (char)toupper(diff[0]);

    char label[96];
    char answered = ctx->answers[idx] ? ctx->answers[idx] : '-';
    snprintf(label, sizeof(label), "Q%d [%c] • %s", idx + 1, answered, diff[0] ? diff : "Medium");
    if (question_buttons[idx]) {
        gtk_button_set_label(GTK_BUTTON(question_buttons[idx]), label);
    }
}

static void rebuild_question_list(ClientContext* ctx) {
    if (!question_list_box) return;

    // Clear existing widgets
    GList *children = gtk_container_get_children(GTK_CONTAINER(question_list_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    for (int i = 0; i < MAX_QUESTIONS; i++) question_buttons[i] = NULL;

    for (int i = 0; i < ctx->question_count; i++) {
        if (!difficulty_matches(ctx->questions[i].difficulty)) continue;
        GtkWidget *q_btn = gtk_button_new_with_label("Q");
        g_signal_connect(q_btn, "clicked", G_CALLBACK(on_question_selected), GINT_TO_POINTER(i));
        gtk_style_context_add_class(gtk_widget_get_style_context(q_btn), "btn-secondary");
        gtk_box_pack_start(GTK_BOX(question_list_box), q_btn, FALSE, FALSE, 0);
        question_buttons[i] = q_btn;
        update_question_button_label(ctx, i);
    }

    gtk_widget_show_all(question_list_box);
}

static void on_filter_changed(GtkComboBoxText *combo, gpointer data) {
    (void)combo;
    ClientContext* ctx = (ClientContext*)data;
    if (!ctx) return;
    gchar* sel = gtk_combo_box_text_get_active_text(combo);
    if (sel) {
        strncpy(difficulty_filter, sel, sizeof(difficulty_filter) - 1);
        difficulty_filter[sizeof(difficulty_filter) - 1] = '\0';
        for (char* p = difficulty_filter; *p; ++p) *p = (char)tolower(*p);
        g_free(sel);
    } else {
        strncpy(difficulty_filter, "all", sizeof(difficulty_filter) - 1);
    }
    rebuild_question_list(ctx);
}

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
    
    char q_text[600];
    char diff[16];
    strncpy(diff, q->difficulty, sizeof(diff) - 1);
    diff[sizeof(diff) - 1] = '\0';
    if (diff[0]) diff[0] = (char)toupper(diff[0]);
    snprintf(q_text, sizeof(q_text), "Question %d (%s): %.500s", 
             ctx->current_question + 1, diff[0] ? diff : "Medium", q->question);
    gtk_label_set_text(GTK_LABEL(question_label), q_text);
    
    // Update radio buttons
    char cur_answer = ctx->answers[ctx->current_question];
    for (int i = 0; i < 4; i++) {
        if (radio_buttons[i]) {
            char label[256];
            char opt = 'A' + i;
            snprintf(label, sizeof(label), "%c. %.250s", opt, q->options[i]);
            gtk_button_set_label(GTK_BUTTON(radio_buttons[i]), label);
            
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_buttons[i]), 
                                        cur_answer == opt);
        }
    }

    if (progress_label) {
        char progress_text[64];
        snprintf(progress_text, sizeof(progress_text), "Progress: %d / %d", ctx->current_question + 1,
                 ctx->question_count > 0 ? ctx->question_count : 1);
        gtk_label_set_text(GTK_LABEL(progress_label), progress_text);
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
        
        // Save answer to server for reconnect support
        if (ctx->connected && !ctx->is_practice && ctx->current_room_id > 0) {
            char save_msg[64];
            snprintf(save_msg, sizeof(save_msg), "%d,%d,%c", 
                     ctx->current_room_id, ctx->current_question, answer);
            client_send_message(ctx, "SAVE_ANSWER", save_msg);
        }
        
        update_question_button_label(ctx, ctx->current_question);
    }
}

static void on_submit_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->question_count == 0) return;
    
    char answer_str[MAX_QUESTIONS + 1];
    for (int i = 0; i < ctx->question_count; i++) {
        answer_str[i] = ctx->answers[i] ? ctx->answers[i] : '-';
    }
    answer_str[ctx->question_count] = '\0';
    
    if (ctx->is_practice) {
        // Score locally for practice mode
        int score = 0;
        int total = ctx->question_count;
        
        // Parse practice answers
        char practice_copy[256];
        strncpy(practice_copy, ctx->practice_answers, sizeof(practice_copy) - 1);
        practice_copy[sizeof(practice_copy) - 1] = '\0';
        
        char* saveptr;
        char* token = strtok_r(practice_copy, ",", &saveptr);
        int idx = 0;
        while (token && idx < ctx->question_count) {
            if (ctx->answers[idx] == token[0]) {
                score++;
            }
            idx++;
            token = strtok_r(NULL, ",", &saveptr);
        }
        
        // Calculate time taken
        time_t now = time(NULL);
        ctx->time_taken = ctx->quiz_start_time > 0 ? (int)(now - ctx->quiz_start_time) : 0;
        
        ctx->score = score;
        ctx->total_questions = total;
        ctx->current_state = PAGE_RESULT;
        strcpy(ctx->status_message, "Practice completed!");
        
        // Navigate to result
        ui_navigate_to_page(PAGE_RESULT);
    } else if (ctx->connected) {
        // Send to server for online mode
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
    // Reset statics to avoid stale pointers across navigations
    status_label = NULL;
    timer_label = NULL;
    question_label = NULL;
    for (int i = 0; i < 4; i++) radio_buttons[i] = NULL;
    question_list_box = NULL;
    for (int i = 0; i < MAX_QUESTIONS; i++) question_buttons[i] = NULL;
    filter_combo = NULL;
    submit_btn = NULL;
    start_btn = NULL;
    waiting_label = NULL;
    progress_label = NULL;
    radio_group = NULL;
    timer_id = 0;
    strncpy(difficulty_filter, "all", sizeof(difficulty_filter) - 1);

    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(page), "page");
    
    // Sidebar
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(sidebar, 200, -1);
    gtk_widget_set_margin_start(sidebar, 10);
    gtk_widget_set_margin_end(sidebar, 10);
    gtk_widget_set_margin_top(sidebar, 10);
    gtk_widget_set_margin_bottom(sidebar, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "card");
    
    GtkWidget *sidebar_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sidebar_title), "<b>QUIZ</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), sidebar_title, FALSE, FALSE, 0);
    
    char room_text[64];
    snprintf(room_text, sizeof(room_text), "Room: %d", ctx->current_room_id);
    GtkWidget *room_label = gtk_label_new(room_text);
    gtk_box_pack_start(GTK_BOX(sidebar), room_label, FALSE, FALSE, 0);
    
    char player_text[80];
    snprintf(player_text, sizeof(player_text), "Player: %.63s", ctx->username);
    GtkWidget *player_label = gtk_label_new(player_text);
    gtk_box_pack_start(GTK_BOX(sidebar), player_label, FALSE, FALSE, 0);
    
    timer_label = gtk_label_new("Time: --:--");
    gtk_style_context_add_class(gtk_widget_get_style_context(timer_label), "timer-chip");
    gtk_box_pack_start(GTK_BOX(sidebar), timer_label, FALSE, FALSE, 5);
    
    char progress_text[64];
    snprintf(progress_text, sizeof(progress_text), "Progress: %d / %d", 
             ctx->current_question + 1, ctx->question_count > 0 ? ctx->question_count : 1);
    progress_label = gtk_label_new(progress_text);
    gtk_style_context_add_class(gtk_widget_get_style_context(progress_label), "progress-label");
    gtk_box_pack_start(GTK_BOX(sidebar), progress_label, FALSE, FALSE, 0);
    
    // Question list
    GtkWidget *q_label = gtk_label_new("Questions:");
    gtk_box_pack_start(GTK_BOX(sidebar), q_label, FALSE, FALSE, 5);

    filter_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "all");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "easy");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_combo), "hard");
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_combo), 0);
    g_signal_connect(filter_combo, "changed", G_CALLBACK(on_filter_changed), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(filter_combo), "entry");
    gtk_box_pack_start(GTK_BOX(sidebar), filter_combo, FALSE, FALSE, 0);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_style_context_add_class(gtk_widget_get_style_context(scrolled), "card");
    
    question_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(scrolled), question_list_box);
    gtk_box_pack_start(GTK_BOX(sidebar), scrolled, TRUE, TRUE, 0);
    
    GtkWidget *leave_btn = gtk_button_new_with_label("Leave Quiz");
    g_signal_connect(leave_btn, "clicked", G_CALLBACK(on_leave_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(leave_btn), "btn-ghost");
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
    gtk_style_context_add_class(gtk_widget_get_style_context(main_area), "card");
    
    status_label = gtk_label_new(ctx->status_message);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-bar");
    gtk_box_pack_start(GTK_BOX(main_area), status_label, FALSE, FALSE, 0);
    
    if (ctx->question_count == 0) {
        if (ctx->quiz_available) {
            waiting_label = gtk_label_new(NULL);
            char markup[256];
            snprintf(markup, sizeof(markup), 
                    "<b>Quiz is ready!</b>\n\nTime limit: %d seconds", ctx->quiz_duration);
            gtk_label_set_markup(GTK_LABEL(waiting_label), markup);
            gtk_style_context_add_class(gtk_widget_get_style_context(waiting_label), "status-bar");
            gtk_box_pack_start(GTK_BOX(main_area), waiting_label, TRUE, FALSE, 0);
            
            start_btn = gtk_button_new_with_label("Start Quiz");
            gtk_widget_set_size_request(start_btn, 150, 40);
            g_signal_connect(start_btn, "clicked", G_CALLBACK(on_start_quiz_clicked), ctx);
            gtk_style_context_add_class(gtk_widget_get_style_context(start_btn), "btn-primary");
            gtk_box_pack_start(GTK_BOX(main_area), start_btn, FALSE, FALSE, 0);
        } else {
            waiting_label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(waiting_label), 
                               "<b>Waiting for quiz to start...</b>\n\nWaiting for host to start the game...");
            gtk_style_context_add_class(gtk_widget_get_style_context(waiting_label), "status-bar");
            gtk_box_pack_start(GTK_BOX(main_area), waiting_label, TRUE, FALSE, 0);
        }
    } else {
        // Display question
        question_label = gtk_label_new("");
        gtk_label_set_line_wrap(GTK_LABEL(question_label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(question_label), 80);
        gtk_widget_set_halign(question_label, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(question_label), "question-card");
        gtk_box_pack_start(GTK_BOX(main_area), question_label, FALSE, FALSE, 10);
        
        // Answer options
        radio_group = NULL;
        for (int i = 0; i < 4; i++) {
            radio_buttons[i] = gtk_radio_button_new_with_label(radio_group, "");
            radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_buttons[i]));
            g_signal_connect(radio_buttons[i], "toggled", 
                           G_CALLBACK(on_answer_selected), GINT_TO_POINTER('A' + i));
            gtk_style_context_add_class(gtk_widget_get_style_context(radio_buttons[i]), "answer-option");
            gtk_box_pack_start(GTK_BOX(main_area), radio_buttons[i], FALSE, FALSE, 5);
        }
        
        submit_btn = gtk_button_new_with_label("Submit Answers");
        gtk_widget_set_size_request(submit_btn, 150, 40);
        g_signal_connect(submit_btn, "clicked", G_CALLBACK(on_submit_clicked), ctx);
        gtk_style_context_add_class(gtk_widget_get_style_context(submit_btn), "btn-primary");
        gtk_box_pack_end(GTK_BOX(main_area), submit_btn, FALSE, FALSE, 10);
        
        rebuild_question_list(ctx);
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
