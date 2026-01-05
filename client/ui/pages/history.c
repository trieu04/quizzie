#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_HISTORY_ENTRIES 100

typedef struct {
    char filename[128];
    time_t timestamp;
    char quiz_type[16];    // "Practice" or "Exam"
    char subject[32];
    int score;
    int total;
    float percentage;
    int time_taken;        // in seconds
} HistoryEntry;

static HistoryEntry history_entries[MAX_HISTORY_ENTRIES];
static int history_count = 0;
static GtkWidget *history_list_box = NULL;
static GtkWidget *filter_type_combo = NULL;
static char current_filter[16] = "all";

// Parse a result filename: username_type_subject_timestamp.txt
static bool parse_result_filename(const char* filename, HistoryEntry* entry) {
    if (!filename || !entry) return false;
    
    // Copy filename
    strncpy(entry->filename, filename, sizeof(entry->filename) - 1);
    entry->filename[sizeof(entry->filename) - 1] = '\0';
    
    // Parse: username_type_subject_timestamp.txt
    char buf[128];
    strncpy(buf, filename, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    // Remove .txt extension
    char* dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
    
    // Split by underscores
    char* parts[4];
    int part_count = 0;
    char* token = strtok(buf, "_");
    while (token && part_count < 4) {
        parts[part_count++] = token;
        token = strtok(NULL, "_");
    }
    
    if (part_count < 4) return false;
    
    // parts[0] = username, parts[1] = type, parts[2] = subject, parts[3] = timestamp
    strncpy(entry->quiz_type, parts[1], sizeof(entry->quiz_type) - 1);
    entry->quiz_type[sizeof(entry->quiz_type) - 1] = '\0';
    
    strncpy(entry->subject, parts[2], sizeof(entry->subject) - 1);
    entry->subject[sizeof(entry->subject) - 1] = '\0';
    
    entry->timestamp = (time_t)atol(parts[3]);
    
    return true;
}

// Read result file content and extract score info
static bool read_result_file(const char* filepath, HistoryEntry* entry) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) return false;
    
    char line[256];
    entry->score = 0;
    entry->total = 0;
    entry->time_taken = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // Look for "Score: X / Y"
        if (strncmp(line, "Score:", 6) == 0) {
            sscanf(line, "Score: %d / %d", &entry->score, &entry->total);
        }
        // Look for "Time: MM:SS"
        else if (strncmp(line, "Time:", 5) == 0) {
            int mins = 0, secs = 0;
            sscanf(line, "Time: %d:%d", &mins, &secs);
            entry->time_taken = mins * 60 + secs;
        }
    }
    
    fclose(fp);
    
    if (entry->total > 0) {
        entry->percentage = (float)entry->score / entry->total * 100.0f;
    } else {
        entry->percentage = 0.0f;
    }
    
    return entry->total > 0;
}

// Load all history entries for current user
static void load_history(ClientContext* ctx) {
    history_count = 0;
    
    if (!ctx || ctx->username[0] == '\0') return;
    
    const char* result_dirs[] = {
        "data/results",
        "../data/results",
        "../../data/results",
        NULL
    };
    
    DIR* dir = NULL;
    const char* result_dir = NULL;
    
    // Find the results directory
    for (int i = 0; result_dirs[i] != NULL; i++) {
        dir = opendir(result_dirs[i]);
        if (dir) {
            result_dir = result_dirs[i];
            break;
        }
    }
    
    if (!dir) {
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && history_count < MAX_HISTORY_ENTRIES) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if file starts with username_
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "%s_", ctx->username);
        if (strncmp(entry->d_name, prefix, strlen(prefix)) != 0) {
            continue;
        }
        
        // Parse filename
        HistoryEntry hist;
        if (!parse_result_filename(entry->d_name, &hist)) {
            continue;
        }
        
        // Build full path
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", result_dir, entry->d_name);
        
        // Read file content
        if (read_result_file(filepath, &hist)) {
            history_entries[history_count++] = hist;
        }
    }
    
    closedir(dir);
    
    // Sort by timestamp (newest first)
    for (int i = 0; i < history_count - 1; i++) {
        for (int j = i + 1; j < history_count; j++) {
            if (history_entries[i].timestamp < history_entries[j].timestamp) {
                HistoryEntry temp = history_entries[i];
                history_entries[i] = history_entries[j];
                history_entries[j] = temp;
            }
        }
    }
}

static bool entry_matches_filter(HistoryEntry* entry) {
    if (!entry) return false;
    if (strcmp(current_filter, "all") == 0) return true;
    
    // Compare case-insensitive
    char type_lower[16];
    strncpy(type_lower, entry->quiz_type, sizeof(type_lower) - 1);
    type_lower[sizeof(type_lower) - 1] = '\0';
    for (char* p = type_lower; *p; p++) *p = tolower(*p);
    
    return strcmp(type_lower, current_filter) == 0;
}

static void rebuild_history_list(ClientContext* ctx) {
    if (!history_list_box) return;
    
    // Clear existing widgets
    GList *children = gtk_container_get_children(GTK_CONTAINER(history_list_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    if (history_count == 0) {
        GtkWidget *empty_label = gtk_label_new("No history records found.");
        gtk_style_context_add_class(gtk_widget_get_style_context(empty_label), "status-bar");
        gtk_box_pack_start(GTK_BOX(history_list_box), empty_label, FALSE, FALSE, 10);
        gtk_widget_show_all(history_list_box);
        return;
    }
    
    // Display each entry
    int displayed = 0;
    for (int i = 0; i < history_count; i++) {
        if (!entry_matches_filter(&history_entries[i])) continue;
        
        HistoryEntry* entry = &history_entries[i];
        
        // Create frame for each entry
        GtkWidget *entry_frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(entry_frame), GTK_SHADOW_NONE);
        gtk_style_context_add_class(gtk_widget_get_style_context(entry_frame), "card");
        
        GtkWidget *entry_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_set_margin_start(entry_box, 10);
        gtk_widget_set_margin_end(entry_box, 10);
        gtk_widget_set_margin_top(entry_box, 10);
        gtk_widget_set_margin_bottom(entry_box, 10);
        
        // Header: Type + Subject
        GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        
        char header_text[128];
        snprintf(header_text, sizeof(header_text), "<b>%s - %s</b>", 
                entry->quiz_type, entry->subject);
        GtkWidget *header_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(header_label), header_text);
        gtk_widget_set_halign(header_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(header_box), header_label, FALSE, FALSE, 0);
        
        // Date/Time
        char time_str[64];
        struct tm* timeinfo = localtime(&entry->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", timeinfo);
        GtkWidget *time_label = gtk_label_new(time_str);
        gtk_widget_set_halign(time_label, GTK_ALIGN_END);
        gtk_widget_set_hexpand(time_label, TRUE);
        gtk_box_pack_end(GTK_BOX(header_box), time_label, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(entry_box), header_box, FALSE, FALSE, 0);
        
        // Score and percentage
        char score_text[128];
        snprintf(score_text, sizeof(score_text), "Score: %d / %d (%.1f%%)", 
                entry->score, entry->total, entry->percentage);
        GtkWidget *score_label = gtk_label_new(score_text);
        gtk_widget_set_halign(score_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(entry_box), score_label, FALSE, FALSE, 0);
        
        // Progress bar
        GtkWidget *progress = gtk_level_bar_new_for_interval(0.0, 100.0);
        gtk_level_bar_set_value(GTK_LEVEL_BAR(progress), entry->percentage);
        gtk_widget_set_size_request(progress, -1, 10);
        gtk_box_pack_start(GTK_BOX(entry_box), progress, FALSE, FALSE, 0);
        
        // Time taken
        if (entry->time_taken > 0) {
            int mins = entry->time_taken / 60;
            int secs = entry->time_taken % 60;
            char time_taken_str[64];
            snprintf(time_taken_str, sizeof(time_taken_str), "Time: %02d:%02d", mins, secs);
            GtkWidget *time_taken_label = gtk_label_new(time_taken_str);
            gtk_widget_set_halign(time_taken_label, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(entry_box), time_taken_label, FALSE, FALSE, 0);
        }
        
        gtk_container_add(GTK_CONTAINER(entry_frame), entry_box);
        gtk_box_pack_start(GTK_BOX(history_list_box), entry_frame, FALSE, FALSE, 5);
        displayed++;
    }
    
    if (displayed == 0) {
        GtkWidget *empty_label = gtk_label_new("No records match the current filter.");
        gtk_style_context_add_class(gtk_widget_get_style_context(empty_label), "status-bar");
        gtk_box_pack_start(GTK_BOX(history_list_box), empty_label, FALSE, FALSE, 10);
    }
    
    gtk_widget_show_all(history_list_box);
}

static void on_filter_type_changed(GtkComboBoxText *combo, gpointer data) {
    ClientContext* ctx = (ClientContext*)data;
    if (!ctx) return;
    
    gchar* sel = gtk_combo_box_text_get_active_text(combo);
    if (sel) {
        strncpy(current_filter, sel, sizeof(current_filter) - 1);
        current_filter[sizeof(current_filter) - 1] = '\0';
        for (char* p = current_filter; *p; p++) *p = tolower(*p);
        g_free(sel);
    } else {
        strncpy(current_filter, "all", sizeof(current_filter) - 1);
    }
    
    rebuild_history_list(ctx);
}

static void on_back_to_dashboard_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_DASHBOARD;
    ui_navigate_to_page(PAGE_DASHBOARD);
}

GtkWidget* page_history_create(ClientContext* ctx) {
    // Reset statics
    history_list_box = NULL;
    filter_type_combo = NULL;
    strncpy(current_filter, "all", sizeof(current_filter) - 1);
    
    // Load history entries
    load_history(ctx);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");
    
    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(header, 20);
    gtk_widget_set_margin_bottom(header, 20);
    
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>Test History</span>");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 5);
    
    char subtitle[128];
    snprintf(subtitle, sizeof(subtitle), "<span size='large'>Results for %s</span>", ctx->username);
    GtkWidget *subtitle_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(subtitle_label), subtitle);
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle_label), "header-subtitle");
    gtk_box_pack_start(GTK_BOX(header), subtitle_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);
    
    // Filter section
    GtkWidget *filter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(filter_box, 30);
    gtk_widget_set_margin_end(filter_box, 30);
    gtk_widget_set_margin_bottom(filter_box, 10);
    
    GtkWidget *filter_label = gtk_label_new("Filter by type:");
    gtk_box_pack_start(GTK_BOX(filter_box), filter_label, FALSE, FALSE, 0);
    
    filter_type_combo = GTK_WIDGET(gtk_combo_box_text_new());
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_type_combo), "All");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_type_combo), "Practice");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filter_type_combo), "Exam");
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_type_combo), 0);
    g_signal_connect(filter_type_combo, "changed", G_CALLBACK(on_filter_type_changed), ctx);
    gtk_box_pack_start(GTK_BOX(filter_box), filter_type_combo, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), filter_box, FALSE, FALSE, 0);
    
    // Scrolled window for history list
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_margin_start(scrolled, 30);
    gtk_widget_set_margin_end(scrolled, 30);
    gtk_widget_set_margin_bottom(scrolled, 20);
    
    history_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(scrolled), history_list_box);
    
    gtk_box_pack_start(GTK_BOX(main_box), scrolled, TRUE, TRUE, 0);
    
    // Build the history list
    rebuild_history_list(ctx);
    
    // Back button
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(button_box, 30);
    gtk_widget_set_margin_end(button_box, 30);
    gtk_widget_set_margin_bottom(button_box, 20);
    
    GtkWidget *back_btn = gtk_button_new_with_label("Back to Dashboard");
    gtk_widget_set_size_request(back_btn, 200, 45);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_to_dashboard_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(back_btn), "btn-primary");
    gtk_box_pack_start(GTK_BOX(button_box), back_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);
    
    return main_box;
}

void page_history_update(ClientContext* ctx) {
    // Refresh history if needed
    (void)ctx;
}
