#include "ui.h"
#include "ui_login.h"
#include "ui_home.h"
#include "ui_admin.h"
#include "net.h"
#include "protocol.h"
#include "window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *window = NULL; // Main active window
static GtkWidget *status_label = NULL;
static int sock = -1;
static char current_username[32];
static guint network_watch_id = 0;

// Forward declarations
gboolean on_network_event(GIOChannel *source, GIOCondition condition, gpointer data);
static void update_status(const char *msg);

void ui_init(int *argc, char ***argv) {
    gtk_init(argc, argv);
    printf("UI initialized.\n");
}

int ui_get_socket() {
    return sock;
}

static void transition_window() {
    clear_window_signals(window);
    if (window) {
        gtk_widget_destroy(window);
        window = NULL;
    }
    status_label = NULL;
}

void ui_show_login() {
    transition_window();
    ui_show_login_window(&window, &status_label);
}

void ui_show_home(const char *username) {
    transition_window();
    ui_show_home_window(&window, &status_label, username);
}

static void show_message(const char *msg, GtkMessageType type) {
    if (!window) return;
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           type,
                                           GTK_BUTTONS_OK,
                                           "%s", msg);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void update_status(const char *msg) {
    if (status_label && GTK_IS_LABEL(status_label)) {
        gtk_label_set_text(GTK_LABEL(status_label), msg);
    }
}

static void disconnect_client(gboolean show_error) {
    if (network_watch_id > 0) {
        g_source_remove(network_watch_id);
        network_watch_id = 0;
    }
    
    if (sock != -1) {
        close(sock);
        sock = -1;
    }
    
    update_status("Status: Disconnected");
    
    if (show_error && window) {
        show_message("Disconnected from server.", GTK_MESSAGE_WARNING);
    }
    
    ui_show_login();
}

int ensure_connection(const char *ip, int port) {
    if (sock >= 0) return 0; 

    sock = net_connect(ip, port);
    if (sock >= 0) {
        update_status("Status: Connected");
        
        GIOChannel *channel = g_io_channel_unix_new(sock);
        network_watch_id = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, on_network_event, NULL);
        g_io_channel_unref(channel);
        return 0;
    } else {
        update_status("Status: Connection Failed");
        show_message("Could not connect to server.", GTK_MESSAGE_ERROR);
        return -1;
    }
}

void login_controller_on_login(const char *ip, int port, const char *username, const char *password) {
    if (ensure_connection(ip, port) < 0) return;
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        show_message("Please enter username and password.", GTK_MESSAGE_INFO);
        return;
    }
    
    strncpy(current_username, username, sizeof(current_username) - 1);
    current_username[sizeof(current_username) - 1] = '\0';

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_LOGIN);
    
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, JSON_KEY_USERNAME, username);
    cJSON_AddStringToObject(data, JSON_KEY_PASSWORD, password);
    cJSON_AddItemToObject(req, JSON_KEY_DATA, data);

    if (send_packet(sock, MSG_TYPE_REQ, req) < 0) {
        update_status("Status: Send Failed");
    }
    cJSON_Delete(req);
}

void home_controller_on_logout() {
    if (sock < 0) return;
     
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_LOGOUT);
    cJSON_AddStringToObject(req, JSON_KEY_DATA, "{}"); 
    send_packet(sock, MSG_TYPE_REQ, req);
    cJSON_Delete(req);
    
    // Clean disconnect without error helper
    disconnect_client(FALSE);
}

gboolean send_heartbeat(gpointer data) {
    (void)data;
    if (sock != -1) {
        cJSON *hbt = cJSON_CreateObject();
        if (send_packet(sock, MSG_TYPE_HBT, hbt) < 0) {
            cJSON_Delete(hbt);
            disconnect_client(TRUE);
            return FALSE;
        }
        cJSON_Delete(hbt);
        return TRUE;
    }
    return FALSE;
}

static void handle_server_message(char *msg_type, cJSON *payload) {
    cJSON *message_item = cJSON_GetObjectItem(payload, JSON_KEY_MESSAGE);
    const char *msg = message_item ? message_item->valuestring : "";
    
    if (strcmp(msg_type, MSG_TYPE_RES) == 0) {
         if (strcmp(msg, "Login successful") == 0) {
             cJSON *data = cJSON_GetObjectItem(payload, JSON_KEY_DATA);
             cJSON *role_item = cJSON_GetObjectItem(data, "role");
             const char *role = role_item ? role_item->valuestring : "participant";
             
             if (strcmp(role, "admin") == 0) {
                 ui_show_admin_dashboard(&window, &status_label, current_username);
             } else {
                 ui_show_home(current_username);
             }
             g_timeout_add(5000, send_heartbeat, NULL);
         } else {
             // Check if it is a room list response (data is array)
             cJSON *data = cJSON_GetObjectItem(payload, JSON_KEY_DATA);
             if (data && cJSON_IsArray(data)) {
                 ui_admin_update_room_list(data);
             } 
             else if (msg && strlen(msg) > 0) {
                 show_message(msg, GTK_MESSAGE_INFO);
             }
         }
    } else if (strcmp(msg_type, MSG_TYPE_ERR) == 0) {
         show_message(msg, GTK_MESSAGE_ERROR);
    } 
}

gboolean on_network_event(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)source;
    (void)data;
    
    if (sock == -1) return FALSE; 

    if (condition & G_IO_HUP) {
        disconnect_client(TRUE);
        return FALSE; 
    }

    if (condition & G_IO_IN) {
        char msg_type[4];
        cJSON *payload = NULL;
        int res = receive_packet(sock, msg_type, &payload);

        if (res == 0) {
            handle_server_message(msg_type, payload);
            cJSON_Delete(payload);
        } else {
             // Read error -> Disconnect
             disconnect_client(TRUE);
             return FALSE;
        }
    }
    return TRUE;
}
