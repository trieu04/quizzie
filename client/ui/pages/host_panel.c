#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static GtkWidget *status_label = NULL;
static GtkWidget *state_label = NULL;
static GtkWidget *duration_entry = NULL;
static GtkWidget *filename_entry = NULL;
static GtkWidget *participant_list_box = NULL;
static GtkWidget *stats_label = NULL;
static GtkWidget *avg_level_bar = NULL;
static GtkWidget *avg_detail_label = NULL;
static GtkWidget *start_btn = NULL;
static time_t last_stats_request = 0;
static guint stats_timer_id = 0;

static gboolean request_stats_periodically(gpointer data) {
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->room_state == QUIZ_STATE_STARTED && ctx->connected) {
        time_t now = time(NULL);
        if (now - last_stats_request >= 2) {
            client_send_message(ctx, "GET_STATS", "");
            last_stats_request = now;
        }
    }
    
    return G_SOURCE_CONTINUE;
}

static void update_participant_list(ClientContext* ctx) {
    if (!participant_list_box) return;
    
    // Clear existing items
    GList *children = gtk_container_get_children(GTK_CONTAINER(participant_list_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    if (ctx->participant_count == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "No participants yet. Share Room ID: %d", ctx->current_room_id);
        GtkWidget *label = gtk_label_new(msg);
        gtk_box_pack_start(GTK_BOX(participant_list_box), label, FALSE, FALSE, 5);
        gtk_widget_show_all(participant_list_box);
        return;
    }
    
    for (int i = 0; i < ctx->participant_count; i++) {
        ParticipantInfo* p = &ctx->participants[i];
        
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_margin_start(row, 5);
        gtk_widget_set_margin_end(row, 5);
        gtk_widget_set_margin_top(row, 3);
        gtk_widget_set_margin_bottom(row, 3);
        gtk_style_context_add_class(gtk_widget_get_style_context(row), "participant-row");
        
        const char* status_str = "Waiting";
        char info_str[64] = "";
        
        if (p->status == 'T') {
            status_str = "Taking";
            int mins = p->remaining_time / 60;
            int secs = p->remaining_time % 60;
            snprintf(info_str, sizeof(info_str), "%02d:%02d left", mins, secs);
        } else if (p->status == 'S') {
            status_str = "Submitted";
            snprintf(info_str, sizeof(info_str), "%d/%d", p->score, p->total);
        }
        
        char participant_info[256];
        snprintf(participant_info, sizeof(participant_info), "%-20s | %-12s | %s",
                 p->username, status_str, info_str);
        
        GtkWidget *label = gtk_label_new(participant_info);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), label, TRUE, TRUE, 0);
        
        gtk_box_pack_start(GTK_BOX(participant_list_box), row, FALSE, FALSE, 0);
    }
    
    gtk_widget_show_all(participant_list_box);
}

static void on_start_quiz_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected && ctx->room_state == QUIZ_STATE_WAITING) {
        client_send_message(ctx, "START_GAME", "");
        strcpy(ctx->status_message, "Starting quiz...");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
    }
}

static void on_refresh_stats_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected) {
        client_send_message(ctx, "GET_STATS", "");
        strcpy(ctx->status_message, "Refreshing stats...");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
        last_stats_request = time(NULL);
    }
}

static void on_set_duration_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->room_state != QUIZ_STATE_WAITING) {
        strcpy(ctx->status_message, "Cannot change after quiz started");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
        return;
    }
    
    const char* duration_str = gtk_entry_get_text(GTK_ENTRY(duration_entry));
    int duration = atoi(duration_str);
    
    if (duration > 0) {
        ctx->quiz_duration = duration;
        char config[128];
        snprintf(config, sizeof(config), "%d,%s", ctx->quiz_duration, ctx->question_file);
        client_send_message(ctx, "SET_CONFIG", config);
        strcpy(ctx->status_message, "Duration updated");
    } else {
        strcpy(ctx->status_message, "Invalid duration");
    }
    
    if (status_label) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}

static void on_set_file_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->room_state != QUIZ_STATE_WAITING) {
        strcpy(ctx->status_message, "Cannot change after quiz started");
        if (status_label) {
            gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        }
        return;
    }
    
    const char* filename = gtk_entry_get_text(GTK_ENTRY(filename_entry));
    
    if (strlen(filename) > 0) {
        strncpy(ctx->question_file, filename, sizeof(ctx->question_file) - 1);
        ctx->question_file[sizeof(ctx->question_file) - 1] = '\0';
        
        char config[128];
        snprintf(config, sizeof(config), "%d,%s", ctx->quiz_duration, ctx->question_file);
        client_send_message(ctx, "SET_CONFIG", config);
        strcpy(ctx->status_message, "File updated");
    } else {
        strcpy(ctx->status_message, "Invalid filename");
    }
    
    if (status_label) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
}

static void on_leave_room_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    ctx->current_room_id = -1;
    ctx->is_host = false;
    ctx->room_state = QUIZ_STATE_WAITING;
    ctx->participant_count = 0;
    AppState target = ctx->role == 1 ? PAGE_ADMIN_PANEL : PAGE_DASHBOARD;
    ctx->current_state = target;
    
    if (stats_timer_id > 0) {
        g_source_remove(stats_timer_id);
        stats_timer_id = 0;
    }
    
    ui_navigate_to_page(target);
}

static void on_delete_room_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    if (ctx->connected && ctx->current_room_id > 0) {
        char msg[32];
        sprintf(msg, "%d", ctx->current_room_id);
        client_send_message(ctx, "DELETE_ROOM", msg);
        
        ctx->current_room_id = -1;
        ctx->is_host = false;
        ctx->room_state = QUIZ_STATE_WAITING;
        ctx->participant_count = 0;
        AppState target = ctx->role == 1 ? PAGE_ADMIN_PANEL : PAGE_DASHBOARD;
        ctx->current_state = target;
        
        if (stats_timer_id > 0) {
            g_source_remove(stats_timer_id);
            stats_timer_id = 0;
        }
        
        ui_navigate_to_page(target);
    }
}

GtkWidget* page_host_panel_create(ClientContext* ctx) {
    status_label = NULL;
    state_label = NULL;
    duration_entry = NULL;
    filename_entry = NULL;
    participant_list_box = NULL;
    stats_label = NULL;
    avg_level_bar = NULL;
    avg_detail_label = NULL;
    start_btn = NULL;
    last_stats_request = 0;
    // stats_timer_id cleared in cleanup when leaving; ensure not duplicated
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(page), "page");
    
    // Sidebar
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(sidebar, 250, -1);
    gtk_widget_set_margin_start(sidebar, 10);
    gtk_widget_set_margin_end(sidebar, 10);
    gtk_widget_set_margin_top(sidebar, 10);
    gtk_widget_set_margin_bottom(sidebar, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(sidebar), "card");
    
    GtkWidget *sidebar_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sidebar_title), "<b>HOST PANEL</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), sidebar_title, FALSE, FALSE, 5);
    
    char room_text[64];
    snprintf(room_text, sizeof(room_text), "Room ID: %d", ctx->current_room_id);
    GtkWidget *room_label = gtk_label_new(room_text);
    gtk_box_pack_start(GTK_BOX(sidebar), room_label, FALSE, FALSE, 0);
    
    char host_text[64];
    snprintf(host_text, sizeof(host_text), "Host: %s", ctx->username);
    GtkWidget *host_label = gtk_label_new(host_text);
    gtk_box_pack_start(GTK_BOX(sidebar), host_label, FALSE, FALSE, 0);
    
    // Status
    GtkWidget *status_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(status_title), "<b>Status:</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), status_title, FALSE, FALSE, 10);
    
    const char* state_str = "Waiting";
    if (ctx->room_state == QUIZ_STATE_STARTED) state_str = "Quiz Active";
    else if (ctx->room_state == QUIZ_STATE_FINISHED) state_str = "Finished";
    state_label = gtk_label_new(state_str);
    gtk_style_context_add_class(gtk_widget_get_style_context(state_label), "pill");
    gtk_style_context_add_class(gtk_widget_get_style_context(state_label),
        ctx->room_state == QUIZ_STATE_STARTED ? "pill-warn" : "pill-success");
    gtk_box_pack_start(GTK_BOX(sidebar), state_label, FALSE, FALSE, 0);
    
    // Configuration
    GtkWidget *config_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(config_title), "<b>Configuration:</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), config_title, FALSE, FALSE, 10);
    
    GtkWidget *duration_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *duration_label = gtk_label_new("Duration (s):");
    duration_entry = gtk_entry_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(duration_entry), "entry");
    char dur_str[16];
    snprintf(dur_str, sizeof(dur_str), "%d", ctx->quiz_duration);
    gtk_entry_set_text(GTK_ENTRY(duration_entry), dur_str);
    gtk_entry_set_width_chars(GTK_ENTRY(duration_entry), 10);
    GtkWidget *set_dur_btn = gtk_button_new_with_label("Set");
    g_signal_connect(set_dur_btn, "clicked", G_CALLBACK(on_set_duration_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(set_dur_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(duration_box), duration_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(duration_box), duration_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(duration_box), set_dur_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), duration_box, FALSE, FALSE, 5);
    
    GtkWidget *file_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *file_label = gtk_label_new("Question File:");
    filename_entry = gtk_entry_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(filename_entry), "entry");
    gtk_entry_set_text(GTK_ENTRY(filename_entry), ctx->question_file);
    GtkWidget *set_file_btn = gtk_button_new_with_label("Set File");
    g_signal_connect(set_file_btn, "clicked", G_CALLBACK(on_set_file_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(set_file_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(file_box), file_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(file_box), filename_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(file_box), set_file_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sidebar), file_box, FALSE, FALSE, 5);
    
    // Actions
    GtkWidget *actions_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(actions_title), "<b>Actions:</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), actions_title, FALSE, FALSE, 10);
    
    if (ctx->room_state == QUIZ_STATE_WAITING) {
        start_btn = gtk_button_new_with_label("Start Quiz");
        g_signal_connect(start_btn, "clicked", G_CALLBACK(on_start_quiz_clicked), ctx);
        gtk_style_context_add_class(gtk_widget_get_style_context(start_btn), "btn-primary");
        gtk_box_pack_start(GTK_BOX(sidebar), start_btn, FALSE, FALSE, 5);
    } else {
        GtkWidget *refresh_btn = gtk_button_new_with_label("Refresh Stats");
        g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_stats_clicked), ctx);
        gtk_style_context_add_class(gtk_widget_get_style_context(refresh_btn), "btn-secondary");
        gtk_box_pack_start(GTK_BOX(sidebar), refresh_btn, FALSE, FALSE, 5);
    }
    
    GtkWidget *leave_btn = gtk_button_new_with_label("Leave Room");
    g_signal_connect(leave_btn, "clicked", G_CALLBACK(on_leave_room_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(leave_btn), "btn-ghost");
    gtk_box_pack_end(GTK_BOX(sidebar), leave_btn, FALSE, FALSE, 0);
    
    GtkWidget *delete_btn = gtk_button_new_with_label("Delete Room");
    g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_delete_room_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(delete_btn), "btn-secondary");
    gtk_box_pack_end(GTK_BOX(sidebar), delete_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(page), sidebar, FALSE, FALSE, 0);
    
    // Separator
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(page), separator, FALSE, FALSE, 0);
    
    // Main area
    GtkWidget *main_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_area, 20);
    gtk_widget_set_margin_end(main_area, 20);
    gtk_widget_set_margin_top(main_area, 20);
    gtk_widget_set_margin_bottom(main_area, 20);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_area), "card");
    
    status_label = gtk_label_new(ctx->status_message);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-bar");
    gtk_box_pack_start(GTK_BOX(main_area), status_label, FALSE, FALSE, 0);
    
    GtkWidget *stats_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(stats_title), "<span size='large' weight='bold'>PARTICIPANTS STATISTICS</span>");
    gtk_box_pack_start(GTK_BOX(main_area), stats_title, FALSE, FALSE, 5);
    
    char stats_text[256];
    snprintf(stats_text, sizeof(stats_text), 
             "Waiting: %d | Taking: %d | Submitted: %d | Total: %d",
             ctx->stats_waiting, ctx->stats_taking, ctx->stats_submitted,
             ctx->stats_waiting + ctx->stats_taking + ctx->stats_submitted);
    stats_label = gtk_label_new(stats_text);
    gtk_style_context_add_class(gtk_widget_get_style_context(stats_label), "status-bar");
    gtk_box_pack_start(GTK_BOX(main_area), stats_label, FALSE, FALSE, 5);

    GtkWidget *graph_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *graph_title = gtk_label_new("Average Score");
    gtk_widget_set_halign(graph_title, GTK_ALIGN_START);
    avg_level_bar = gtk_level_bar_new_for_interval(0.0, 100.0);
    gtk_level_bar_set_value(GTK_LEVEL_BAR(avg_level_bar), ctx->stats_avg_percent);
    gtk_widget_set_size_request(avg_level_bar, -1, 22);
    gtk_style_context_add_class(gtk_widget_get_style_context(avg_level_bar), "progress-bar");

    char avg_detail[128];
    snprintf(avg_detail, sizeof(avg_detail), "Avg: %d%% | Best: %d%% | Last: %d%%",
             ctx->stats_avg_percent, ctx->stats_best_percent, ctx->stats_last_percent);
    avg_detail_label = gtk_label_new(avg_detail);
    gtk_style_context_add_class(gtk_widget_get_style_context(avg_detail_label), "progress-label");
    gtk_widget_set_halign(avg_detail_label, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(graph_box), graph_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(graph_box), avg_level_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(graph_box), avg_detail_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_area), graph_box, FALSE, FALSE, 5);
    
    GtkWidget *separator2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(main_area), separator2, FALSE, FALSE, 5);
    
    // Participant list
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    participant_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled), participant_list_box);
    gtk_box_pack_start(GTK_BOX(main_area), scrolled, TRUE, TRUE, 0);
    
    update_participant_list(ctx);
    
    gtk_box_pack_start(GTK_BOX(page), main_area, TRUE, TRUE, 0);
    
    // Start periodic stats updates if quiz is running
    if (ctx->room_state == QUIZ_STATE_STARTED) {
        stats_timer_id = g_timeout_add(2000, request_stats_periodically, ctx);
    }
    
    return page;
}

void page_host_panel_update(ClientContext* ctx) {
    update_participant_list(ctx);
    
    if (stats_label) {
        char stats_text[256];
        snprintf(stats_text, sizeof(stats_text), 
                 "Waiting: %d | Taking: %d | Submitted: %d | Total: %d",
                 ctx->stats_waiting, ctx->stats_taking, ctx->stats_submitted,
                 ctx->stats_waiting + ctx->stats_taking + ctx->stats_submitted);
        gtk_label_set_text(GTK_LABEL(stats_label), stats_text);
    }

    if (avg_level_bar) {
        gtk_level_bar_set_value(GTK_LEVEL_BAR(avg_level_bar), ctx->stats_avg_percent);
    }
    if (avg_detail_label) {
        char avg_detail[128];
        snprintf(avg_detail, sizeof(avg_detail), "Avg: %d%% | Best: %d%% | Last: %d%%",
                 ctx->stats_avg_percent, ctx->stats_best_percent, ctx->stats_last_percent);
        gtk_label_set_text(GTK_LABEL(avg_detail_label), avg_detail);
    }
    
    if (status_label && strlen(ctx->status_message) > 0) {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
    }
    
    if (state_label) {
        const char* state_str = "Waiting";
        if (ctx->room_state == QUIZ_STATE_STARTED) state_str = "Quiz Active";
        else if (ctx->room_state == QUIZ_STATE_FINISHED) state_str = "Finished";
        gtk_label_set_text(GTK_LABEL(state_label), state_str);
    }
}

void page_host_panel_cleanup() {
    if (stats_timer_id > 0) {
        g_source_remove(stats_timer_id);
        stats_timer_id = 0;
    }
}
