#include "ui.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

// UI Widgets
static GtkWidget *duration_spin = NULL;
static GtkWidget *start_time_entry = NULL;
static GtkWidget *end_time_entry = NULL;
static GtkWidget *file_combo = NULL;
static GtkWidget *easy_spin = NULL;
static GtkWidget *med_spin = NULL;
static GtkWidget *hard_spin = NULL;
static GtkWidget *rand_check = NULL;
static GtkWidget *status_label = NULL;

// Stats labels
static GtkWidget *val_easy = NULL;
static GtkWidget *val_med = NULL;
static GtkWidget *val_hard = NULL;
static GtkWidget *val_total = NULL;

static void on_cancel_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    ui_navigate_to_page(PAGE_ADMIN_PANEL);
}

static void on_create_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    // Get values
    int duration = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(duration_spin));
    
    int active_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(file_combo));
    if (active_idx < 0) {
        strncpy(ctx->status_message, "Please select a question file.", sizeof(ctx->status_message) - 1);
        ctx->status_message[sizeof(ctx->status_message) - 1] = '\0';
        if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        return;
    }
    const char* filename = ctx->available_files_list[active_idx].name;
    
    int easy = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(easy_spin));
    int med = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(med_spin));
    int hard = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hard_spin));
    
    bool rand = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rand_check));
    
    // Use current system time for start_time and start_time + 10 minutes for end_time
    time_t start_time = time(NULL);
    time_t end_time = start_time + 600;
    
    if (easy + med + hard <= 0) {
        strncpy(ctx->status_message, "Must request at least 1 question.", sizeof(ctx->status_message) - 1);
        ctx->status_message[sizeof(ctx->status_message) - 1] = '\0';
        if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        return;
    }
    
    // Check against available stats (optional client side check)
    QuestionFile* file = &ctx->available_files_list[active_idx];
    if (easy > file->easy_cnt || med > file->med_cnt || hard > file->hard_cnt) {
        strncpy(ctx->status_message, "Not enough questions of requested difficulty.", sizeof(ctx->status_message) - 1);
        ctx->status_message[sizeof(ctx->status_message) - 1] = '\0';
        if (status_label) gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        return;
    }
    
    // Construct config string: 
    // duration|start_time|end_time|filename|easy|med|hard|rand_ans
    char config[512];
    snprintf(config, sizeof(config), "%d|%ld|%ld|%s|%d|%d|%d|%d",
             duration * 60, // Server expects seconds
             start_time, end_time, filename,
             easy, med, hard, rand ? 1 : 0);
             
    // Send CREATE_ROOM
    char message[1024];
    snprintf(message, sizeof(message), "%s,%s", ctx->username, config);
    client_send_message(ctx, "CREATE_ROOM", message);
    
    gtk_label_set_text(GTK_LABEL(status_label), "Creating room...");
}

static void on_file_changed(GtkComboBox *widget, gpointer data) {
    ClientContext* ctx = (ClientContext*)data;
    int idx = gtk_combo_box_get_active(widget);
    if (idx >= 0 && idx < ctx->available_files_count) {
        QuestionFile* f = &ctx->available_files_list[idx];
        char buf[32];
        
        snprintf(buf, sizeof(buf), "%d", f->easy_cnt);
        gtk_label_set_text(GTK_LABEL(val_easy), buf);
        
        snprintf(buf, sizeof(buf), "%d", f->med_cnt);
        gtk_label_set_text(GTK_LABEL(val_med), buf);
        
        snprintf(buf, sizeof(buf), "%d", f->hard_cnt);
        gtk_label_set_text(GTK_LABEL(val_hard), buf);
        
        snprintf(buf, sizeof(buf), "%d", f->total_cnt);
        gtk_label_set_text(GTK_LABEL(val_total), buf);
    }
}

GtkWidget* page_create_room_create(ClientContext* ctx) {
    // Reset all
    duration_spin = NULL; start_time_entry = NULL; end_time_entry = NULL;
    file_combo = NULL; easy_spin = NULL; med_spin = NULL; hard_spin = NULL;
    rand_check = NULL; status_label = NULL;
    val_easy = NULL; val_med = NULL; val_hard = NULL; val_total = NULL;
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");
    
    // Top Toolbar
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(toolbar, 10);
    gtk_widget_set_margin_bottom(toolbar, 10);
    
    // Title Section
    GtkWidget *title = gtk_label_new("Create Quiz Room");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title"); 
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(toolbar), title, FALSE, FALSE, 0);
    
    // Cancel Button (Back) in Toolbar
    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_cancel_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_cancel), "btn-ghost");
    gtk_box_pack_end(GTK_BOX(toolbar), btn_cancel, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), toolbar, FALSE, FALSE, 0);
    
    // Scrollable content
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled, TRUE, TRUE, 0);
    
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_set_margin_start(content, 40);
    gtk_widget_set_margin_end(content, 40);
    gtk_container_add(GTK_CONTAINER(scrolled), content);
    
    GtkWidget *form_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(form_grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(form_grid), 20);
    gtk_box_pack_start(GTK_BOX(content), form_grid, FALSE, FALSE, 0);
    
    int row = 0;
    
    // --- FILE SELECTION ---
    GtkWidget *lbl_file = gtk_label_new("Question File:");
    gtk_widget_set_halign(lbl_file, GTK_ALIGN_END);
    file_combo = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(file_combo, TRUE);
    g_signal_connect(file_combo, "changed", G_CALLBACK(on_file_changed), ctx);
    client_send_message(ctx, "GET_QUESTION_FILES", "");
    ctx->files_refreshed = false;
    
    gtk_grid_attach(GTK_GRID(form_grid), lbl_file, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), file_combo, 1, row++, 1, 1);
    
    // File Stats
    GtkWidget *stats_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    
    // Easy
    GtkWidget *box_e = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(box_e), gtk_label_new("Easy"), FALSE, FALSE, 0);
    val_easy = gtk_label_new("0");
    gtk_widget_set_name(val_easy, "stat-val");
    gtk_box_pack_start(GTK_BOX(box_e), val_easy, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stats_box), box_e, FALSE, FALSE, 0);

    // Medium
    GtkWidget *box_m = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(box_m), gtk_label_new("Med"), FALSE, FALSE, 0);
    val_med = gtk_label_new("0");
    gtk_box_pack_start(GTK_BOX(box_m), val_med, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stats_box), box_m, FALSE, FALSE, 0);

    // Hard
    GtkWidget *box_h = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(box_h), gtk_label_new("Hard"), FALSE, FALSE, 0);
    val_hard = gtk_label_new("0");
    gtk_box_pack_start(GTK_BOX(box_h), val_hard, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stats_box), box_h, FALSE, FALSE, 0);
    
    // Total
    GtkWidget *box_t = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(box_t), gtk_label_new("Total"), FALSE, FALSE, 0);
    val_total = gtk_label_new("0");
    gtk_box_pack_start(GTK_BOX(box_t), val_total, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(stats_box), box_t, FALSE, FALSE, 0);
    
    gtk_grid_attach(GTK_GRID(form_grid), stats_box, 1, row++, 1, 1);
    
    // Separator
    gtk_grid_attach(GTK_GRID(form_grid), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, row++, 2, 1);
    
    // --- TIMING ---
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    
    GtkWidget *lbl_start = gtk_label_new("Start Time:");
    gtk_widget_set_halign(lbl_start, GTK_ALIGN_END);
    start_time_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(start_time_entry), buf);
    gtk_grid_attach(GTK_GRID(form_grid), lbl_start, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), start_time_entry, 1, row++, 1, 1);
    
    GtkWidget *lbl_end = gtk_label_new("End Time:");
    gtk_widget_set_halign(lbl_end, GTK_ALIGN_END);
    end_time_entry = gtk_entry_new();
    // Default end time + 10 minutes
    tm->tm_min += 10;
    mktime(tm); // normalize
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    gtk_entry_set_text(GTK_ENTRY(end_time_entry), buf);
    
    gtk_grid_attach(GTK_GRID(form_grid), lbl_end, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), end_time_entry, 1, row++, 1, 1);
    
    GtkWidget *lbl_dur = gtk_label_new("Quiz Limit (mins):");
    gtk_widget_set_halign(lbl_dur, GTK_ALIGN_END);
    duration_spin = gtk_spin_button_new_with_range(1, 180, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(duration_spin), 5);
    gtk_grid_attach(GTK_GRID(form_grid), lbl_dur, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), duration_spin, 1, row++, 1, 1);
    
    // Separator
    gtk_grid_attach(GTK_GRID(form_grid), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, row++, 2, 1);
    
    // --- CONFIG ---
    GtkWidget *lbl_easy = gtk_label_new("Easy Count:");
    gtk_widget_set_halign(lbl_easy, GTK_ALIGN_END);
    easy_spin = gtk_spin_button_new_with_range(0, 50, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(easy_spin), 0);
    gtk_grid_attach(GTK_GRID(form_grid), lbl_easy, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), easy_spin, 1, row++, 1, 1);
    
    GtkWidget *lbl_med = gtk_label_new("Medium Count:");
    gtk_widget_set_halign(lbl_med, GTK_ALIGN_END);
    med_spin = gtk_spin_button_new_with_range(0, 50, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(med_spin), 0);
    gtk_grid_attach(GTK_GRID(form_grid), lbl_med, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), med_spin, 1, row++, 1, 1);
    
    GtkWidget *lbl_hard = gtk_label_new("Hard Count:");
    gtk_widget_set_halign(lbl_hard, GTK_ALIGN_END);
    hard_spin = gtk_spin_button_new_with_range(0, 50, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(hard_spin), 0);
    gtk_grid_attach(GTK_GRID(form_grid), lbl_hard, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), hard_spin, 1, row++, 1, 1);
    
    rand_check = gtk_check_button_new_with_label("Randomize Answer Order");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rand_check), TRUE);
    gtk_grid_attach(GTK_GRID(form_grid), rand_check, 1, row++, 1, 1);
    
    // Status
    status_label = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "error-label");
    gtk_grid_attach(GTK_GRID(form_grid), status_label, 0, row++, 2, 1);
    
     // Buttons
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    
    GtkWidget *btn_create = gtk_button_new_with_label("Create Room");
    g_signal_connect(btn_create, "clicked", G_CALLBACK(on_create_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_create), "btn-primary");
    gtk_widget_set_size_request(btn_create, 150, 45); 
    
    gtk_box_pack_start(GTK_BOX(btn_box), btn_create, FALSE, FALSE, 0);
    
    gtk_grid_attach(GTK_GRID(form_grid), btn_box, 0, row++, 2, 1);
    
    return main_box;
}

void page_create_room_update(ClientContext* ctx) {
    if (ctx->files_refreshed && file_combo) {
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(file_combo));
        
        for (int i=0; i < ctx->available_files_count; i++) {
             gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(file_combo), ctx->available_files_list[i].name);
        }
        
        if (ctx->available_files_count > 0) gtk_combo_box_set_active(GTK_COMBO_BOX(file_combo), 0);
        else gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(file_combo), "No files found");
        
        ctx->files_refreshed = false;
    }
    
    // Handle status messages from server
    if (status_label && ctx->status_message[0] != '\0') {
        gtk_label_set_text(GTK_LABEL(status_label), ctx->status_message);
        
        // Add color styling based on message type
        const char* msg = ctx->status_message;
        if (strstr(msg, "failed") || strstr(msg, "Failed") || strstr(msg, "error") || 
            strstr(msg, "Error") || strstr(msg, "Invalid") || strstr(msg, "cannot") ||
            strstr(msg, "Not enough") || strstr(msg, "Must") || strstr(msg, "Please")) {
            gtk_widget_set_name(status_label, "error-label");
        } else if (strstr(msg, "success") || strstr(msg, "Success") || strstr(msg, "successful") ||
                   strstr(msg, "created") || strstr(msg, "Created")) {
            gtk_widget_set_name(status_label, "success-label");
        } else {
            gtk_widget_set_name(status_label, "info-label");
        }
        // Don't clear message here - let it persist until user takes new action
    }
}
