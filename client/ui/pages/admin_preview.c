#include "ui.h"
#include "net.h"
#include <gtk/gtk.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static GtkWidget *file_listbox = NULL;
static GtkWidget *csv_textview = NULL;
static GtkWidget *file_info_label = NULL;
static char selected_file[256] = "";

static void load_csv_file(const char* filepath) {
    if (!csv_textview) return;
    
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(csv_textview));
        gtk_text_buffer_set_text(buffer, "Error: Could not open file.", -1);
        return;
    }
    
    // Read entire file
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(fp);
        return;
    }
    
    size_t read_size = fread(content, 1, size, fp);
    content[read_size] = '\0';
    fclose(fp);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(csv_textview));
    gtk_text_buffer_set_text(buffer, content, -1);
    
    free(content);
    
    // Update file info
    if (file_info_label) {
        // Count lines
        int line_count = 0;
        FILE* fp2 = fopen(filepath, "r");
        if (fp2) {
            char line[2048];
            while (fgets(line, sizeof(line), fp2)) {
                if (strlen(line) > 1) line_count++;
            }
            fclose(fp2);
        }
        
        char info[256];
        snprintf(info, sizeof(info), "File: %s | Lines: %d | Size: %ld bytes", 
                 strrchr(filepath, '/') ? strrchr(filepath, '/') + 1 : filepath,
                 line_count, size);
        gtk_label_set_text(GTK_LABEL(file_info_label), info);
    }
}

static void on_file_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    (void)data;
    
    if (!row) return;
    
    GtkWidget *label = gtk_bin_get_child(GTK_BIN(row));
    const char* filename = gtk_label_get_text(GTK_LABEL(label));
    
    strncpy(selected_file, filename, sizeof(selected_file) - 1);
    selected_file[sizeof(selected_file) - 1] = '\0';
    
    // Try different paths - includes both exam and practice subdirectories
    const char* question_dirs[] = {
        "data/questions/exam",
        "../data/questions/exam",
        "../../data/questions/exam",
        "data/questions/practice",
        "../data/questions/practice",
        "../../data/questions/practice",
        NULL
    };
    
    for (int i = 0; question_dirs[i] != NULL; i++) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", question_dirs[i], filename);
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            load_csv_file(filepath);
            break;
        }
    }
}

static void load_question_files(ClientContext* ctx) {
    if (!file_listbox) return;
    
    // Clear existing items
    GList *children = gtk_container_get_children(GTK_CONTAINER(file_listbox));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    // Try different paths for exam folder
    const char* question_dirs[] = {
        "data/questions/exam",
        "../data/questions/exam",
        "../../data/questions/exam",
        "data/questions/practice",
        "../data/questions/practice",
        "../../data/questions/practice",
        NULL
    };
    
    DIR* dir = NULL;
    for (int i = 0; question_dirs[i] != NULL; i++) {
        dir = opendir(question_dirs[i]);
        if (dir) break;
    }
    
    if (!dir) {
        return;
    }
    
    struct dirent* entry;
    int file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Only show .csv files
        size_t len = strlen(entry->d_name);
        if (len < 4 || strcmp(entry->d_name + len - 4, ".csv") != 0) {
            continue;
        }
        
        GtkWidget *label = gtk_label_new(entry->d_name);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_widget_set_margin_start(label, 10);
        gtk_widget_set_margin_end(label, 10);
        gtk_widget_set_margin_top(label, 8);
        gtk_widget_set_margin_bottom(label, 8);
        
        gtk_list_box_insert(GTK_LIST_BOX(file_listbox), label, -1);
        file_count++;
    }
    
    closedir(dir);
    
    gtk_widget_show_all(file_listbox);
}

static void on_back_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    ctx->current_state = PAGE_ADMIN_PANEL;
    ui_navigate_to_page(PAGE_ADMIN_PANEL);
}

static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    load_question_files(ctx);
}

GtkWidget* create_file_preview_page(ClientContext* ctx) {
    // Reset statics

    file_listbox = NULL;
    csv_textview = NULL;
    file_info_label = NULL;
    selected_file[0] = '\0';
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "page");

    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(header, 20);
    gtk_widget_set_margin_bottom(header, 15);
    
    GtkWidget *title = gtk_label_new("Question Banks");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "page-title");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new("Browse and preview CSV question files");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "page-subtitle");
    gtk_box_pack_start(GTK_BOX(header), subtitle, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);

    // Main content area - horizontal split
    GtkWidget *content_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_start(content_paned, 20);
    gtk_widget_set_margin_end(content_paned, 20);
    gtk_widget_set_margin_bottom(content_paned, 15);
    gtk_paned_set_position(GTK_PANED(content_paned), 300);  // Set initial divider position
    
    // Left panel - File list
    GtkWidget *left_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(left_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(left_frame), "card");
    GtkWidget *left_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(left_header), "<b>Available Files</b>");
    gtk_frame_set_label_widget(GTK_FRAME(left_frame), left_header);
    
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(left_box, 10);
    gtk_widget_set_margin_end(left_box, 10);
    gtk_widget_set_margin_top(left_box, 10);
    gtk_widget_set_margin_bottom(left_box, 10);
    
    // Refresh button
    GtkWidget *refresh_btn = gtk_button_new_with_label("Refresh List");
    gtk_widget_set_size_request(refresh_btn, -1, 35);
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(refresh_btn), "btn-secondary");
    gtk_box_pack_start(GTK_BOX(left_box), refresh_btn, FALSE, FALSE, 0);
    
    // File list in scrolled window
    GtkWidget *scrolled_list = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_list),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_list, 200, 300);  // Set minimum size
    
    file_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(file_listbox), GTK_SELECTION_SINGLE);
    g_signal_connect(file_listbox, "row-selected", G_CALLBACK(on_file_selected), ctx);
    gtk_style_context_add_class(gtk_widget_get_style_context(file_listbox), "card");
    
    gtk_container_add(GTK_CONTAINER(scrolled_list), file_listbox);
    gtk_box_pack_start(GTK_BOX(left_box), scrolled_list, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(left_frame), left_box);
    gtk_paned_pack1(GTK_PANED(content_paned), left_frame, FALSE, TRUE);  // Allow resize and shrink
    
    // Right panel - CSV preview
    GtkWidget *right_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(right_frame), GTK_SHADOW_NONE);
    gtk_style_context_add_class(gtk_widget_get_style_context(right_frame), "card");
    GtkWidget *right_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(right_header), "<b>File Preview</b>");
    gtk_frame_set_label_widget(GTK_FRAME(right_frame), right_header);
    
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(right_box, 10);
    gtk_widget_set_margin_end(right_box, 10);
    gtk_widget_set_margin_top(right_box, 10);
    gtk_widget_set_margin_bottom(right_box, 10);
    
    // File info label
    file_info_label = gtk_label_new("Select a file to preview");
    gtk_widget_set_halign(file_info_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(right_box), file_info_label, FALSE, FALSE, 0);
    
    // CSV content viewer
    GtkWidget *scrolled_csv = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_csv),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_csv), GTK_SHADOW_IN);
    gtk_widget_set_size_request(scrolled_csv, 400, 300);  // Set minimum size for preview area
    
    csv_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(csv_textview), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(csv_textview), GTK_WRAP_WORD);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(csv_textview), FALSE);
    gtk_widget_set_margin_start(csv_textview, 5);
    gtk_widget_set_margin_end(csv_textview, 5);
    gtk_widget_set_margin_top(csv_textview, 5);
    gtk_widget_set_margin_bottom(csv_textview, 5);
    
    // Set monospace font for better CSV viewing using CSS
    GtkStyleContext *text_ctx = gtk_widget_get_style_context(csv_textview);
    gtk_style_context_add_class(text_ctx, "monospace");
    
    gtk_container_add(GTK_CONTAINER(scrolled_csv), csv_textview);
    gtk_box_pack_start(GTK_BOX(right_box), scrolled_csv, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(right_frame), right_box);
    gtk_paned_pack2(GTK_PANED(content_paned), right_frame, TRUE, TRUE);  // Allow resize and shrink
    
    gtk_box_pack_start(GTK_BOX(main_box), content_paned, TRUE, TRUE, 0);

    // Footer
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(footer_box, 20);
    gtk_widget_set_margin_end(footer_box, 20);
    gtk_widget_set_margin_bottom(footer_box, 15);
    
    GtkWidget *back_btn = gtk_button_new_with_label("← Back to Admin Panel");
    gtk_widget_set_size_request(back_btn, 180, 40);
    gtk_style_context_add_class(gtk_widget_get_style_context(back_btn), "btn-ghost");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(footer_box), back_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), footer_box, FALSE, FALSE, 0);
    
    // Load files on page creation
    load_question_files(ctx);

    return main_box;
}

void page_file_preview_update(ClientContext* ctx) {
    (void)ctx;
    // No updates needed for file preview page
}
