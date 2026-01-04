#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>

// Forward declaration for practice mode
extern int storage_load_questions(const char* filename, char* questions, char* answers);

static GtkWidget *status_label = NULL;
static GtkWidget *subject_combo = NULL;

static const char* get_selected_subject() {
    if (!subject_combo) return "general";
    const char* subj = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(subject_combo));
    return subj ? subj : "general";
}

static void on_practice_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    // Set practice mode flag
    ctx->is_practice = true;
    ctx->current_room_id = -1;
    ctx->is_host = false;
    
    // Load questions locally from file for selected subject
    const char* subject = get_selected_subject();
    strncpy(ctx->subject, subject, sizeof(ctx->subject) - 1);
    ctx->subject[sizeof(ctx->subject) - 1] = '\0';
    char questions[BUFFER_SIZE], answers[256];
    char rel1[256], rel2[256], rel3[256];
    snprintf(rel1, sizeof(rel1), "data/practice_bank_%s.csv", subject);
    snprintf(rel2, sizeof(rel2), "../data/practice_bank_%s.csv", subject);
    snprintf(rel3, sizeof(rel3), "../data/practice_bank_%s.csv", subject);
    const char* paths[] = {rel1, rel2, rel3, NULL};
    int loaded = 0;
    for (int i = 0; paths[i] != NULL; i++) {
        if (storage_load_questions(paths[i], questions, answers) == 0) {
            loaded = 1;
            break;
        }
    }
    
    if (!loaded) {
        strcpy(ctx->status_message, "Failed to load practice questions!");
        if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        return;
    }
    
    // Parse questions
    ctx->question_count = 0;
    memset(ctx->questions, 0, sizeof(ctx->questions));
    memset(ctx->answers, 0, sizeof(ctx->answers));
    
    char buffer[BUFFER_SIZE];
    strncpy(buffer, questions, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';
    
    char* saveptr1;
    char* question_token = strtok_r(buffer, ";", &saveptr1);
    
    while (question_token && ctx->question_count < MAX_QUESTIONS) {
        Question* q = &ctx->questions[ctx->question_count];
        q->id = ctx->question_count + 1;
        
        char* question_mark = strchr(question_token, '?');
        if (question_mark) {
            size_t q_len = question_mark - question_token;
            if (q_len >= sizeof(q->question)) q_len = sizeof(q->question) - 1;
            strncpy(q->question, question_token, q_len);
            q->question[q_len] = '\0';
            
            char* options_str = question_mark + 2;
            char options_copy[512];
            strncpy(options_copy, options_str, sizeof(options_copy) - 1);
            options_copy[sizeof(options_copy) - 1] = '\0';
            
            char* saveptr2;
            char* opt_token = strtok_r(options_copy, "|", &saveptr2);
            int opt_idx = 0;
            while (opt_token && opt_idx < 4) {
                if (strlen(opt_token) > 2 && opt_token[1] == '.') {
                    strncpy(q->options[opt_idx], opt_token + 2, sizeof(q->options[opt_idx]) - 1);
                } else {
                    strncpy(q->options[opt_idx], opt_token, sizeof(q->options[opt_idx]) - 1);
                }
                opt_idx++;
                opt_token = strtok_r(NULL, "|", &saveptr2);
            }
        }
        
        ctx->question_count++;
        question_token = strtok_r(NULL, ";", &saveptr1);
    }
    
    // Store correct answers for local scoring
    strncpy(ctx->practice_answers, answers, sizeof(ctx->practice_answers) - 1);
    
    ctx->total_questions = ctx->question_count;
    ctx->current_question = 0;
    ctx->quiz_duration = 300;
    ctx->quiz_start_time = time(NULL);
    
    // Navigate to quiz page
    ctx->current_state = PAGE_QUIZ;
    ui_navigate_to_page(PAGE_QUIZ);
}

// Removed unused on_create_room_clicked to eliminate -Wunused-function warning

static void on_view_rooms_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_ROOM_LIST;
    ui_navigate_to_page(PAGE_ROOM_LIST);
}

static void on_logout_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    net_close(ctx);
    ctx->username[0] = '\0';
    ctx->current_room_id = -1;
    ctx->status_message[0] = '\0';
    ctx->current_state = PAGE_LOGIN;
    ui_navigate_to_page(PAGE_LOGIN);
}

GtkWidget* page_dashboard_create(ClientContext* ctx) {
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    // Header section
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(header, 20);
    gtk_widget_set_margin_bottom(header, 20);
    
    // Title with emoji
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold' foreground='#2c3e50'>Participant Dashboard</span>");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 5);
    
    // Welcome message
    GtkWidget *welcome_label = gtk_label_new(NULL);
    char welcome[128];
    snprintf(welcome, sizeof(welcome), "<span size='large' foreground='#27ae60'>Welcome, <b>%s</b>!</span>", ctx->username);
    gtk_label_set_markup(GTK_LABEL(welcome_label), welcome);
    gtk_box_pack_start(GTK_BOX(header), welcome_label, FALSE, FALSE, 0);
    
    // Connection status
    GtkWidget *conn_label = gtk_label_new(NULL);
    char conn_text[128];
    if (ctx->connected) {
        snprintf(conn_text, sizeof(conn_text), "<span size='small' foreground='#16a085'>\u2713 Connected to %s:%d</span>", SERVER_IP, SERVER_PORT);
    } else {
        snprintf(conn_text, sizeof(conn_text), "<span size='small' foreground='#e74c3c'>\u2717 Disconnected</span>");
    }
    gtk_label_set_markup(GTK_LABEL(conn_label), conn_text);
    gtk_box_pack_start(GTK_BOX(header), conn_label, FALSE, FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);
    
    // Content area
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(content, 30);
    gtk_widget_set_margin_end(content, 30);
    gtk_widget_set_margin_bottom(content, 30);
    
    // Status message frame
    GtkWidget *status_frame = gtk_frame_new(NULL);
    status_label = gtk_label_new(ctx->status_message);
    gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
    gtk_widget_set_margin_start(status_label, 10);
    gtk_widget_set_margin_end(status_label, 10);
    gtk_widget_set_margin_top(status_label, 8);
    gtk_widget_set_margin_bottom(status_label, 8);
    gtk_container_add(GTK_CONTAINER(status_frame), status_label);
    gtk_box_pack_start(GTK_BOX(content), status_frame, FALSE, FALSE, 0);
    
    // Practice Mode Section
    GtkWidget *practice_frame = gtk_frame_new(NULL);
    GtkWidget *practice_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(practice_header), "<b>[*] Practice Mode</b>");
    gtk_frame_set_label_widget(GTK_FRAME(practice_frame), practice_header);
    gtk_frame_set_shadow_type(GTK_FRAME(practice_frame), GTK_SHADOW_ETCHED_IN);
    
    GtkWidget *practice_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(practice_box, 15);
    gtk_widget_set_margin_end(practice_box, 15);
    gtk_widget_set_margin_top(practice_box, 15);
    gtk_widget_set_margin_bottom(practice_box, 15);
    
    // Subject selector
    GtkWidget *subject_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *subject_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subject_label), "<b>Select Subject:</b>");
    subject_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "general");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "math");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "network");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(subject_combo), "programming");
    gtk_combo_box_set_active(GTK_COMBO_BOX(subject_combo), 0);
    gtk_box_pack_start(GTK_BOX(subject_hbox), subject_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(subject_hbox), subject_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(practice_box), subject_hbox, FALSE, FALSE, 0);
    
    GtkWidget *practice_btn = gtk_button_new_with_label("\u25b6\ufe0f  Start Practice");
    gtk_widget_set_size_request(practice_btn, -1, 45);
    g_signal_connect(practice_btn, "clicked", G_CALLBACK(on_practice_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(practice_box), practice_btn, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(practice_frame), practice_box);
    gtk_box_pack_start(GTK_BOX(content), practice_frame, FALSE, FALSE, 0);
    
    // Online Quiz Section
    GtkWidget *quiz_frame = gtk_frame_new(NULL);
    GtkWidget *quiz_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(quiz_header), "<b>[+] Online Quiz</b>");
    gtk_frame_set_label_widget(GTK_FRAME(quiz_frame), quiz_header);
    gtk_frame_set_shadow_type(GTK_FRAME(quiz_frame), GTK_SHADOW_ETCHED_IN);
    
    GtkWidget *quiz_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(quiz_box, 15);
    gtk_widget_set_margin_end(quiz_box, 15);
    gtk_widget_set_margin_top(quiz_box, 15);
    gtk_widget_set_margin_bottom(quiz_box, 15);
    
    GtkWidget *view_btn = gtk_button_new_with_label(">> Browse Quiz Rooms");
    gtk_widget_set_size_request(view_btn, -1, 45);
    g_signal_connect(view_btn, "clicked", G_CALLBACK(on_view_rooms_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(quiz_box), view_btn, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(quiz_frame), quiz_box);
    gtk_box_pack_start(GTK_BOX(content), quiz_frame, FALSE, FALSE, 0);
    
    // Logout button
    GtkWidget *logout_btn = gtk_button_new_with_label("<< Logout");
    gtk_widget_set_size_request(logout_btn, -1, 40);
    g_signal_connect(logout_btn, "clicked", G_CALLBACK(on_logout_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(content), logout_btn, FALSE, FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(main_box), content, TRUE, TRUE, 0);
    
    return main_box;
}

void page_dashboard_update(ClientContext* ctx) {
    if (status_label && strlen(ctx->status_message) > 0) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}
