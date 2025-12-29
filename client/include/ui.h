#ifndef UI_H
#define UI_H

#include "client.h"
#include <gtk/gtk.h>

// UI context structure
typedef struct {
    GtkWidget *window;
    GtkWidget *main_container;
    GtkWidget *current_page;
    ClientContext *client_ctx;
} UIContext;

// Function prototypes
void ui_init(int *argc, char ***argv);
void ui_cleanup();
void ui_run(ClientContext* ctx);
UIContext* ui_get_context();

// Page creation functions
GtkWidget* page_login_create(ClientContext* ctx);
GtkWidget* page_dashboard_create(ClientContext* ctx);
GtkWidget* page_room_list_create(ClientContext* ctx);
GtkWidget* page_quiz_create(ClientContext* ctx);
GtkWidget* page_result_create(ClientContext* ctx);
GtkWidget* page_host_panel_create(ClientContext* ctx);
GtkWidget* create_admin_panel_page(ClientContext* ctx);

// Page update functions
void page_login_update(ClientContext* ctx);
void page_dashboard_update(ClientContext* ctx);
void page_room_list_update(ClientContext* ctx);
void page_quiz_update(ClientContext* ctx);
void page_result_update(ClientContext* ctx);
void page_host_panel_update(ClientContext* ctx);
void page_admin_panel_update(ClientContext* ctx);

// Page cleanup functions
void page_quiz_cleanup();
void page_host_panel_cleanup();

// Navigation
void ui_navigate_to_page(AppState state);

#endif // UI_H
