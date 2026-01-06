#include "ui.h"
#include "net.h"
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

static void on_logout_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    ClientContext* ctx = (ClientContext*)data;
    
    // Completely disconnect and reset session to ensure fresh state for next login
    net_close(ctx);
    
    // Reset critical session data
    ctx->username[0] = '\0';
    ctx->role = 0;
    ctx->current_room_id = -1;
    ctx->last_room_id = -1;
    ctx->is_host = false;
    ctx->room_state = QUIZ_STATE_WAITING;
    ctx->status_message[0] = '\0';
    
    // Reset stats
    ctx->score = 0;
    ctx->total_questions = 0;
    
    // Clear receive buffers to prevent processing old data on new connection
    ctx->recv_len = 0;
    ctx->recv_buffer[0] = '\0';
    
    ui_navigate_to_page(PAGE_LOGIN);
}

static void create_status_bar(ClientContext* ctx) {
    // Create container
    ui_context.status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkStyleContext *sb_ctx = gtk_widget_get_style_context(ui_context.status_bar);
    gtk_style_context_add_class(sb_ctx, "app-status-bar");
    
    // Username label
    ui_context.label_username = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(ui_context.status_bar), ui_context.label_username, FALSE, FALSE, 10);
    
    // Server info
    ui_context.label_server = gtk_label_new("");
    GtkStyleContext *srv_ctx = gtk_widget_get_style_context(ui_context.label_server);
    gtk_style_context_add_class(srv_ctx, "status-item");
    gtk_box_pack_start(GTK_BOX(ui_context.status_bar), ui_context.label_server, FALSE, FALSE, 10);
    
    // Connection status
    ui_context.label_connection = gtk_label_new("Connecting...");
    GtkStyleContext *conn_ctx = gtk_widget_get_style_context(ui_context.label_connection);
    gtk_style_context_add_class(conn_ctx, "status-item");
    gtk_box_pack_start(GTK_BOX(ui_context.status_bar), ui_context.label_connection, FALSE, FALSE, 10);
    
    // Spacer
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(ui_context.status_bar), spacer, TRUE, TRUE, 0);
    
    // Logout button
    ui_context.btn_logout = gtk_button_new_with_label("Logout");
    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(ui_context.btn_logout);
    gtk_style_context_add_class(btn_ctx, "btn-ghost");
    gtk_style_context_add_class(btn_ctx, "btn-sm"); // will add small button style
    g_signal_connect(ui_context.btn_logout, "clicked", G_CALLBACK(on_logout_clicked), ctx);
    gtk_box_pack_end(GTK_BOX(ui_context.status_bar), ui_context.btn_logout, FALSE, FALSE, 5);
}

void ui_update_status_bar(ClientContext* ctx) {
    if (!ui_context.status_bar) return;
    
    // Update Username
    char user_text[80];
    if (ctx->username[0] != '\0') {
        snprintf(user_text, sizeof(user_text), "User: %.63s", ctx->username);
    } else {
        snprintf(user_text, sizeof(user_text), "Not logged in");
    }
    gtk_label_set_text(GTK_LABEL(ui_context.label_username), user_text);
    
    // Update Server
    char server_text[160];
    snprintf(server_text, sizeof(server_text), "Server: %.120s:%d", ctx->server_ip, ctx->server_port);
    gtk_label_set_text(GTK_LABEL(ui_context.label_server), server_text);
    
    // Update Connection
    GtkStyleContext *conn_ctx = gtk_widget_get_style_context(ui_context.label_connection);
    if (ctx->connected) {
        gtk_label_set_text(GTK_LABEL(ui_context.label_connection), "Connected");
        gtk_style_context_remove_class(conn_ctx, "status-disconnected");
        gtk_style_context_add_class(conn_ctx, "status-connected");
    } else {
        gtk_label_set_text(GTK_LABEL(ui_context.label_connection), "Disconnected");
        gtk_style_context_remove_class(conn_ctx, "status-connected");
        gtk_style_context_add_class(conn_ctx, "status-disconnected");
    }
    
    // Show/Hide Logout - only visible on dashboard pages
    if (ctx->current_state == PAGE_DASHBOARD || ctx->current_state == PAGE_ADMIN_PANEL) {
        gtk_widget_show(ui_context.btn_logout);
    } else {
        gtk_widget_hide(ui_context.btn_logout);
    }
}

static gboolean update_server_messages(gpointer data) {
    ClientContext* ctx = (ClientContext*)data;
    if (ctx) { // Run even if not connected to update "Disconnected" status
        ui_update_status_bar(ctx);
        AppState old_state = ctx->current_state;
        if (ctx->connected) {
            client_receive_message(ctx);
        }
        
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
                case PAGE_REGISTER:
                    page_register_update(ctx);
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
                case PAGE_FILE_PREVIEW:
                    page_file_preview_update(ctx);
                    break;
                case PAGE_CREATE_ROOM:
                    page_create_room_update(ctx);
                    break;
                case PAGE_HISTORY:
                    page_history_update(ctx);
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
    gtk_window_set_default_size(GTK_WINDOW(ui_context.window), 1024, 700);
    gtk_window_set_resizable(GTK_WINDOW(ui_context.window), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(ui_context.window), 0);

    GtkStyleContext *win_ctx = gtk_widget_get_style_context(ui_context.window);
    gtk_style_context_add_class(win_ctx, "app-window");
    
    g_signal_connect(ui_context.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Create main container
    ui_context.main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    // Initialize ClientContext for status bar creation
    // The ctx is passed in ui_run, but we need it here for signal connection? 
    // Wait, ui_init doesn't have ctx. ui_run does.
    // Let's create status bar in ui_run or pass ctx to ui_init.
    // Actually ui_init is called before ui_run.
    // But create_status_bar needs 'ctx' for callback. 
    // We can set callback data later or just use ui_context.client_ctx if global.
    // ui_context.client_ctx is set in ui_run. 
    // Let's defer status bar creation to ui_run or just create it here and set signal later?
    // Or better: Let's create it here, but pass NULL as data, and create a wrapper for callback that uses ui_context.client_ctx.
    
    // Actually simpler: let's move status bar creation to ui_run or just create it here but connect signal in ui_run?
    // Let's just create it here with NULL ctx for now, and re-connect or handle it.
    // Actually, let's just add it to ui_run? No, ui_init sets up window.
    
    // Let's modify ui_run to create the status bar so we have ctx.
    
    GtkStyleContext *main_ctx = gtk_widget_get_style_context(ui_context.main_container);
    gtk_style_context_add_class(main_ctx, "main-area"); // Changed from 'page' to avoid double padding if page class has padding
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
    ctx->current_state = state; // Update state to match UI navigation
    switch(state) {
        case PAGE_LOGIN:
            new_page = page_login_create(ctx);
            break;
        case PAGE_REGISTER:
            new_page = page_register_create(ctx);
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
        case PAGE_FILE_PREVIEW:
            new_page = create_file_preview_page(ctx);
            break;
        case PAGE_CREATE_ROOM:
            new_page = page_create_room_create(ctx);
            break;
        case PAGE_HISTORY:
            new_page = page_history_create(ctx);
            break;
    }
    
    if (new_page) {
        // Add .page class to the content page to ensure it has padding/margin
        GtkStyleContext *page_ctx = gtk_widget_get_style_context(new_page);
        gtk_style_context_add_class(page_ctx, "page");
        
        ui_context.current_page = new_page;
        gtk_box_pack_start(GTK_BOX(ui_context.main_container), new_page, TRUE, TRUE, 0);
        gtk_widget_show_all(ui_context.window);
    }
}

void ui_run(ClientContext* ctx) {
    ui_context.client_ctx = ctx;
    
    // Create status bar now that we have ctx
    create_status_bar(ctx);
    gtk_box_pack_start(GTK_BOX(ui_context.main_container), ui_context.status_bar, FALSE, FALSE, 0);
    
    // Create a container for pages to separate them from status bar
    // Actually we can just pack pages into main_container.
    // But we probably want a scrolling area or similar for pages?
    // For now, just pack directly.
    
    // Navigate to initial page
    ui_navigate_to_page(PAGE_LOGIN);
    
    // Set up timer to check server messages
    g_timeout_add(100, update_server_messages, ctx);
    
    gtk_widget_show_all(ui_context.window);
    gtk_main();
}
