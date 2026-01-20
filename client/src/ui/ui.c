#include "ui.h"
#include "net.h"
#include "protocol.h"
#include "ui_admin.h"
#include "ui_exam.h"
#include "ui_home.h"
#include "ui_login.h"
#include "ui_room_detail.h"
#include "window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget* window = NULL; // Main active window
static GtkWidget* status_label = NULL;
static int sock = -1;
static char current_username[32];
static char current_room_id[32];
static guint network_watch_id = 0;

// Forward declarations
gboolean on_network_event(GIOChannel* source, GIOCondition condition, gpointer data);
static void update_status(const char* msg);

void ui_init(int* argc, char*** argv)
{
    gtk_init(argc, argv);
    printf("UI initialized.\n");
}

int ui_get_socket()
{
    return sock;
}

const char* ui_get_username()
{
    return current_username;
}

static void transition_window()
{
    clear_window_signals(window);
    if (window) {
        gtk_widget_destroy(window);
        window = NULL;
    }
    status_label = NULL;
}

void ui_show_login()
{
    transition_window();
    ui_show_login_window(&window, &status_label);
}

void ui_show_home(const char* username)
{
    transition_window();
    ui_show_home_window(&window, &status_label, username);
}

void ui_show_room_detail(const char* room_id)
{
    transition_window();
    strncpy(current_room_id, room_id, sizeof(current_room_id) - 1);
    current_room_id[sizeof(current_room_id) - 1] = '\0';
    ui_show_room_detail_window(&window, room_id, current_username);
}

void ui_show_exam(const char* room_id, cJSON* questions, int* answers, long start_time, int duration_minutes)
{
    transition_window();
    ui_show_exam_window(&window, room_id, questions, answers, start_time, duration_minutes);
}

static void show_message(const char* msg, GtkMessageType type)
{
    if (!window)
        return;
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, type, GTK_BUTTONS_OK, "%s", msg);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void update_status(const char* msg)
{
    if (status_label && GTK_IS_LABEL(status_label)) {
        gtk_label_set_text(GTK_LABEL(status_label), msg);
    }
}

static void disconnect_client(gboolean show_error)
{
    if (network_watch_id > 0) {
        g_source_remove(network_watch_id);
        network_watch_id = 0;
    }

    if (sock != -1) {
        close(sock);
        sock = -1;
    }

    update_status("Trạng thái: Disconnected");

    if (show_error && window) {
        show_message("Đã ngắt kết nối khỏi máy chủ.", GTK_MESSAGE_WARNING);
    }

    ui_show_login();
}

int ensure_connection(const char* ip, int port)
{
    if (sock >= 0)
        return 0;

    sock = net_connect(ip, port);
    if (sock >= 0) {
        update_status("Trạng thái: Connected");

        GIOChannel* channel = g_io_channel_unix_new(sock);
        network_watch_id = g_io_add_watch(channel, G_IO_IN | G_IO_HUP, on_network_event, NULL);
        g_io_channel_unref(channel);
        return 0;
    } else {
        update_status("Trạng thái: Connection Failed");
        show_message("Không thể kết nối đến máy chủ.", GTK_MESSAGE_ERROR);
        return -1;
    }
}

void login_controller_on_login(const char* ip, int port, const char* username, const char* password)
{
    if (ensure_connection(ip, port) < 0)
        return;

    if (strlen(username) == 0 || strlen(password) == 0) {
        show_message("Vui lòng nhập tài khoản và mật khẩu.", GTK_MESSAGE_INFO);
        return;
    }

    strncpy(current_username, username, sizeof(current_username) - 1);
    current_username[sizeof(current_username) - 1] = '\0';

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_LOGIN);

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, JSON_KEY_USERNAME, username);
    cJSON_AddStringToObject(data, JSON_KEY_PASSWORD, password);
    cJSON_AddItemToObject(req, JSON_KEY_DATA, data);

    if (send_packet(sock, MSG_TYPE_REQ, req) < 0) {
        update_status("Status: Send Failed");
    }
    cJSON_Delete(req);
}

void login_controller_on_register(const char* ip, int port, const char* username, const char* password)
{
    if (ensure_connection(ip, port) < 0)
        return;

    if (strlen(username) == 0 || strlen(password) == 0) {
        show_message("Vui lòng nhập tài khoản và mật khẩu.", GTK_MESSAGE_INFO);
        return;
    }

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_REGISTER);

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, JSON_KEY_USERNAME, username);
    cJSON_AddStringToObject(data, JSON_KEY_PASSWORD, password);
    cJSON_AddItemToObject(req, JSON_KEY_DATA, data);

    if (send_packet(sock, MSG_TYPE_REQ, req) < 0) {
        update_status("Status: Send Failed");
    }
    cJSON_Delete(req);
}

void home_controller_on_logout()
{
    if (sock < 0)
        return;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_LOGOUT);
    cJSON_AddStringToObject(req, JSON_KEY_DATA, "{}");
    send_packet(sock, MSG_TYPE_REQ, req);
    cJSON_Delete(req);

    // Clean disconnect without error helper
    disconnect_client(FALSE);
}

gboolean send_heartbeat(gpointer data)
{
    (void)data;
    if (sock != -1) {
        cJSON* hbt = cJSON_CreateObject();
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

static void handle_server_message(char* msg_type, cJSON* payload)
{
    cJSON* message_item = cJSON_GetObjectItem(payload, JSON_KEY_MESSAGE);
    const char* msg = message_item ? message_item->valuestring : "";

    if (strcmp(msg_type, MSG_TYPE_RES) == 0) {
        cJSON* data = cJSON_GetObjectItem(payload, JSON_KEY_DATA);

        if (strcmp(msg, "Login successful") == 0) {
            cJSON* role_item = cJSON_GetObjectItem(data, "role");
            const char* role = role_item ? role_item->valuestring : "participant";

            if (strcmp(role, "admin") == 0) {
                transition_window();
                ui_show_admin_dashboard(&window, &status_label, current_username);
            } else {
                ui_show_home(current_username);
            }
            g_timeout_add(5000, send_heartbeat, NULL);
        } else if (strcmp(msg, "Register successful") == 0) {
            show_message("Đăng ký thành công! Bạn có thể đăng nhập ngay bây giờ.", GTK_MESSAGE_INFO);
        } else if (data) {
            // Handle different response types based on data structure

            // Check if it's a room list
            if (cJSON_IsArray(data)) {
                // Could be admin room list or participant room list
                ui_admin_update_room_list(data);
                home_update_room_list(data);
            }
            // Check if JOIN_ROOM
            else if (cJSON_HasObjectItem(data, "questions")) {
                cJSON* questions = cJSON_GetObjectItem(data, "questions");
                cJSON* answers = cJSON_GetObjectItem(data, "answers");
                cJSON* start_time = cJSON_GetObjectItem(data, "start_time");
                cJSON* duration = cJSON_GetObjectItem(data, "duration");

                if (questions && answers && start_time && duration) {
                    int num_q = cJSON_GetArraySize(questions);
                    int* answer_array = malloc(sizeof(int) * num_q);
                    for (int i = 0; i < num_q; i++) {
                        cJSON* ans = cJSON_GetArrayItem(answers, i);
                        answer_array[i] = ans && cJSON_IsNumber(ans) ? (int)ans->valuedouble : -1;
                    }

                    ui_show_exam(current_room_id, questions, answer_array, (long)start_time->valuedouble, duration->valueint);
                }
            }
            // Check if it's a room stats response
            else if (cJSON_HasObjectItem(data, "results")) {
                // Update room detail view
                extern void room_detail_update_info(cJSON * room_data);
                room_detail_update_info(data);
            }
            // Check if it's an exam result response
            else if (cJSON_HasObjectItem(data, "score")) {
                cJSON* score = cJSON_GetObjectItem(data, "score");
                cJSON* correct_count = cJSON_GetObjectItem(data, "correct_count");
                cJSON* total_questions = cJSON_GetObjectItem(data, "total_questions");

                char result_msg[256];
                snprintf(result_msg, sizeof(result_msg),
                    "Hoàn thành bài thi!\n\nĐiểm: %d%%\nĐúng: %d/%d",
                    score ? score->valueint : 0,
                    correct_count ? correct_count->valueint : 0,
                    total_questions ? total_questions->valueint : 0);

                show_message(result_msg, GTK_MESSAGE_INFO);
                ui_show_home(current_username);
            } else if (msg && strlen(msg) > 0) {
                show_message(msg, GTK_MESSAGE_INFO);
            }
        } else if (msg && strlen(msg) > 0) {
            show_message(msg, GTK_MESSAGE_INFO);
        }
    } else if (strcmp(msg_type, MSG_TYPE_ERR) == 0) {
        show_message(msg, GTK_MESSAGE_ERROR);
    }
}

gboolean on_network_event(GIOChannel* source, GIOCondition condition, gpointer data)
{
    (void)source;
    (void)data;

    if (sock == -1)
        return FALSE;

    if (condition & G_IO_HUP) {
        disconnect_client(TRUE);
        return FALSE;
    }

    if (condition & G_IO_IN) {
        char msg_type[4];
        cJSON* payload = NULL;
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
