#include "ui.h"
#include <gtk/gtk.h>

static UIContext ui_context = {0};

static void ui_load_css(void) {
    const char* candidates[] = {
        "client/ui/theme.css",
        "ui/theme.css",
        "theme.css",
        "../client/ui/theme.css",
        "../ui/theme.css",
        NULL
    };

    GtkCssProvider *provider = gtk_css_provider_new();
    gboolean loaded = FALSE;

    for (int i = 0; candidates[i] != NULL; i++) {
        if (g_file_test(candidates[i], G_FILE_TEST_EXISTS)) {
            if (gtk_css_provider_load_from_path(provider, candidates[i], NULL)) {
                loaded = TRUE;
                break;
            }
        }
    }

    if (loaded) {
        GdkScreen *screen = gdk_screen_get_default();
        gtk_style_context_add_provider_for_screen(screen,
            GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    }

    g_object_unref(provider);
}

UIContext* ui_get_context() {
    return &ui_context;
}

static gboolean update_server_messages(gpointer data) {
    ClientContext* ctx = (ClientContext*)data;
    if (ctx && ctx->connected) {
        AppState old_state = ctx->current_state;
        client_receive_message(ctx);
        
        // Check if state changed or force refresh requested
        if (old_state != ctx->current_state || ctx->force_page_refresh) {
            ctx->force_page_refresh = false;
            ui_navigate_to_page(ctx->current_state);
        } else {
            // Update current page
            switch(ctx->current_state) {
                case PAGE_LOGIN:
                    page_login_update(ctx);
                    break;
                case PAGE_DASHBOARD:
                    page_dashboard_update(ctx);
                    break;
                case PAGE_PRACTICE:
                    page_practice_update(ctx);
                    break;
                case PAGE_ROOM_LIST:
                    page_room_list_update(ctx);
                    break;
                case PAGE_QUIZ:
                    page_quiz_update(ctx);
                    break;
                case PAGE_RESULT:
                    page_result_update(ctx);
                    break;
                case PAGE_HOST_PANEL:
                    page_host_panel_update(ctx);
                    break;
                case PAGE_ADMIN_PANEL:
                    page_admin_panel_update(ctx);
                    break;
                case PAGE_ADMIN_UPLOAD:
                    page_admin_upload_update(ctx);
                    break;
            }
        }
    }
    return G_SOURCE_CONTINUE;
}

void ui_init(int *argc, char ***argv) {
    gtk_init(argc, argv);

    ui_load_css();
    
    // Create main window
    ui_context.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ui_context.window), "Quizzie Client");
    gtk_window_set_default_size(GTK_WINDOW(ui_context.window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(ui_context.window), 0);

    GtkStyleContext *win_ctx = gtk_widget_get_style_context(ui_context.window);
    gtk_style_context_add_class(win_ctx, "app-window");
    
    g_signal_connect(ui_context.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Create main container
    ui_context.main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkStyleContext *main_ctx = gtk_widget_get_style_context(ui_context.main_container);
    gtk_style_context_add_class(main_ctx, "page");
    gtk_container_add(GTK_CONTAINER(ui_context.window), ui_context.main_container);
}

void ui_cleanup() {
    if (ui_context.window) {
        gtk_widget_destroy(ui_context.window);
    }
}

void ui_navigate_to_page(AppState state) {
    ClientContext* ctx = ui_context.client_ctx;
    if (!ctx) return;
    
    // Cleanup old page resources before destroying
    if (ui_context.current_page) {
        if (ctx->current_state == PAGE_QUIZ) {
            page_quiz_cleanup();
        } else if (ctx->current_state == PAGE_HOST_PANEL) {
            page_host_panel_cleanup();
        }
    }
    
    // Remove old page (guard against invalid widget)
    if (ui_context.current_page) {
        if (GTK_IS_WIDGET(ui_context.current_page)) {
            gtk_widget_destroy(ui_context.current_page);
        }
        ui_context.current_page = NULL;
    }
    
    // Create new page
    GtkWidget* new_page = NULL;
    switch(state) {
        case PAGE_LOGIN:
            new_page = page_login_create(ctx);
            break;
        case PAGE_DASHBOARD:
            new_page = page_dashboard_create(ctx);
            break;
        case PAGE_PRACTICE:
            new_page = page_practice_create(ctx);
            break;
        case PAGE_ROOM_LIST:
            new_page = page_room_list_create(ctx);
            break;
        case PAGE_QUIZ:
            new_page = page_quiz_create(ctx);
            break;
        case PAGE_RESULT:
            new_page = page_result_create(ctx);
            break;
        case PAGE_HOST_PANEL:
            new_page = page_host_panel_create(ctx);
            break;
        case PAGE_ADMIN_PANEL:
            new_page = create_admin_panel_page(ctx);
            break;
        case PAGE_ADMIN_UPLOAD:
            new_page = create_admin_upload_page(ctx);
            break;
    }
    
    if (new_page) {
        ui_context.current_page = new_page;
        gtk_box_pack_start(GTK_BOX(ui_context.main_container), new_page, TRUE, TRUE, 0);
        gtk_widget_show_all(ui_context.window);
    }
}

void ui_run(ClientContext* ctx) {
    ui_context.client_ctx = ctx;
    
    // Navigate to initial page
    ui_navigate_to_page(PAGE_LOGIN);
    
    // Set up timer to check server messages
    g_timeout_add(100, update_server_messages, ctx);
    
    gtk_widget_show_all(ui_context.window);
    gtk_main();
}
