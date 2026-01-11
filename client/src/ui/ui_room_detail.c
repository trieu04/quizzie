#include "ui_room_detail.h"
#include "net.h"
#include "protocol.h"
#include "ui.h"
#include "window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct
{
    char room_id[32];
    char username[32];
    GtkWidget* window;
    GtkWidget* info_label;
    GtkWidget* results_scroll;
    GtkWidget* start_button;
    int has_active_exam;
} RoomDetailState;

static RoomDetailState* current_room_detail = NULL;

static void on_start_exam_clicked(GtkWidget* widget, gpointer data);
static void on_back_clicked(GtkWidget* widget, gpointer data);

void ui_show_room_detail_window(GtkWidget** window_out, const char* room_id, const char* username)
{
    GtkWidget* window = create_window("Room Details", 700, 600);
    *window_out = window;

    if (current_room_detail) {
        free(current_room_detail);
    }

    current_room_detail = malloc(sizeof(RoomDetailState));
    memset(current_room_detail, 0, sizeof(RoomDetailState));
    strncpy(current_room_detail->room_id, room_id, sizeof(current_room_detail->room_id) - 1);
    strncpy(current_room_detail->username, username, sizeof(current_room_detail->username) - 1);
    current_room_detail->window = window;
    current_room_detail->has_active_exam = 0;

    // Main layout
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Title
    GtkWidget* title_label = gtk_label_new("Room Information");
    PangoAttrList* attrlist = pango_attr_list_new();
    PangoAttribute* attr = pango_attr_scale_new(1.5);
    pango_attr_list_insert(attrlist, attr);
    gtk_label_set_attributes(GTK_LABEL(title_label), attrlist);
    pango_attr_list_unref(attrlist);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 10);

    // Room info area
    current_room_detail->info_label = gtk_label_new("Loading room information...");
    gtk_label_set_line_wrap(GTK_LABEL(current_room_detail->info_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(current_room_detail->info_label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), current_room_detail->info_label, FALSE, FALSE, 10);

    // Separator
    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 10);

    // Results history area with table
    GtkWidget* results_title = gtk_label_new("Your Results History:");
    gtk_label_set_xalign(GTK_LABEL(results_title), 0.0);
    PangoAttrList* results_attrlist = pango_attr_list_new();
    PangoAttribute* results_attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(results_attrlist, results_attr);
    gtk_label_set_attributes(GTK_LABEL(results_title), results_attrlist);
    pango_attr_list_unref(results_attrlist);
    gtk_box_pack_start(GTK_BOX(vbox), results_title, FALSE, FALSE, 5);

    current_room_detail->results_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(current_room_detail->results_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(current_room_detail->results_scroll, -1, 200);
    gtk_box_pack_start(GTK_BOX(vbox), current_room_detail->results_scroll, TRUE, TRUE, 5);

    // Action buttons
    GtkWidget* btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* btn_back = gtk_button_new_with_label("Back");
    current_room_detail->start_button = gtk_button_new_with_label("Start Quiz");

    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_back_clicked), NULL);
    g_signal_connect(current_room_detail->start_button, "clicked", G_CALLBACK(on_start_exam_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(btn_box), btn_back, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_box), current_room_detail->start_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 10);

    // Request room details and results from server
    int sock = ui_get_socket();
    if (sock >= 0) {
        // Request room stats (includes results)
        cJSON* req = cJSON_CreateObject();
        cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_GET_ROOM_STATS);
        cJSON* data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "room_id", room_id);
        cJSON_AddItemToObject(req, JSON_KEY_DATA, data);
        send_packet(sock, MSG_TYPE_REQ, req);
        cJSON_Delete(req);
    }

    gtk_widget_show_all(window);
}

void room_detail_update_info(cJSON* room_data)
{
    if (!current_room_detail || !room_data)
        return;

    // Extract room info from nested structure
    cJSON* room_info = cJSON_GetObjectItem(room_data, "room");
    if (!room_info) {
        room_info = room_data; // Fallback if the data is not nested
    }

    cJSON* name = cJSON_GetObjectItem(room_info, "name");
    cJSON* id = cJSON_GetObjectItem(room_info, "id");
    cJSON* status = cJSON_GetObjectItem(room_info, "status");
    cJSON* start_time = cJSON_GetObjectItem(room_info, "start_time");
    cJSON* end_time = cJSON_GetObjectItem(room_info, "end_time");
    cJSON* num_questions = cJSON_GetObjectItem(room_info, "num_questions");
    cJSON* allowed_attempts = cJSON_GetObjectItem(room_info, "allowed_attempts");
    cJSON* duration = cJSON_GetObjectItem(room_info, "duration");
    cJSON* results = cJSON_GetObjectItem(room_data, "results");
    cJSON* has_active_exam_obj = cJSON_GetObjectItem(room_data, "has_active_exam");

    char info_text[1024];
    snprintf(info_text, sizeof(info_text),
        "Room Name: %s\n"
        "Room ID: %s\n"
        "Status: %s\n"
        "Number of Questions: %d\n"
        "Duration: %d minutes\n"
        "Allowed Attempts: %d\n"
        "Open Time: %s"
        "Close Time: %s",
        name && cJSON_IsString(name) ? name->valuestring : "N/A",
        id && cJSON_IsString(id) ? id->valuestring : "N/A",
        status && cJSON_IsString(status) ? status->valuestring : "N/A",
        num_questions && cJSON_IsNumber(num_questions) ? num_questions->valueint : 0,
        duration && cJSON_IsNumber(duration) ? duration->valueint : 0,
        allowed_attempts && cJSON_IsNumber(allowed_attempts) ? allowed_attempts->valueint : 0,
        start_time && cJSON_IsNumber(start_time) ? ctime((time_t*)&start_time->valuedouble) : "N/A\n",
        end_time && cJSON_IsNumber(end_time) ? ctime((time_t*)&end_time->valuedouble) : "N/A\n");

    gtk_label_set_text(GTK_LABEL(current_room_detail->info_label), info_text);

    // Update button text based on exam state
    if (has_active_exam_obj && cJSON_IsBool(has_active_exam_obj) && cJSON_IsTrue(has_active_exam_obj)) {
        current_room_detail->has_active_exam = 1;
        gtk_button_set_label(GTK_BUTTON(current_room_detail->start_button), "Continue Exam");
    } else {
        current_room_detail->has_active_exam = 0;
        gtk_button_set_label(GTK_BUTTON(current_room_detail->start_button), "Start Quiz");
    }

    // Create results table
    GtkListStore* store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    if (results && cJSON_IsArray(results)) {
        int user_result_count = 0;
        cJSON* result = NULL;
        cJSON_ArrayForEach(result, results)
        {
            cJSON* username_obj = cJSON_GetObjectItem(result, "username");
            if (username_obj && cJSON_IsString(username_obj) && strcmp(username_obj->valuestring, current_room_detail->username) == 0) {
                cJSON* timestamp_obj = cJSON_GetObjectItem(result, "timestamp");
                cJSON* num_questions_obj = cJSON_GetObjectItem(result, "num_questions");
                cJSON* correct_count_obj = cJSON_GetObjectItem(result, "correct_count");

                time_t ts = timestamp_obj && cJSON_IsNumber(timestamp_obj) ? (time_t)timestamp_obj->valuedouble : 0;
                char datetime_str[64] = "N/A";
                if (ts > 0) {
                    struct tm* tm_info = localtime(&ts);
                    strftime(datetime_str, sizeof(datetime_str), "%Y-%m-%d %H:%M:%S", tm_info);
                }

                int num_q = num_questions_obj && cJSON_IsNumber(num_questions_obj) ? num_questions_obj->valueint : 0;
                int correct = correct_count_obj && cJSON_IsNumber(correct_count_obj) ? correct_count_obj->valueint : 0;

                GtkTreeIter iter;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter,
                    0, datetime_str,
                    1, num_q,
                    2, num_q, // All questions are submitted when exam is finished
                    3, correct,
                    -1);
                user_result_count++;
            }
        }

        if (user_result_count == 0) {
            // Show "No previous attempts" message
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                0, "No previous attempts",
                1, 0,
                2, 0,
                3, 0,
                -1);
        }
    } else {
        // Show "No previous attempts" message
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0, "No previous attempts",
            1, 0,
            2, 0,
            3, 0,
            -1);
    }

    // Create tree view
    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    // Add columns
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Date & Time", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Questions", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Submitted", renderer, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Correct", renderer, "text", 3, NULL);

    // Clear previous content and add new tree view
    GList* children = gtk_container_get_children(GTK_CONTAINER(current_room_detail->results_scroll));
    for (GList* iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    gtk_container_add(GTK_CONTAINER(current_room_detail->results_scroll), tree_view);
    gtk_widget_show_all(current_room_detail->results_scroll);
}

static void on_start_exam_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;

    if (!current_room_detail)
        return;

    const char* button_text = current_room_detail->has_active_exam ? "continue the quiz" : "start the quiz";
    char message[256];
    snprintf(message, sizeof(message),
        "Are you ready to %s?\n\n"
        "Once started, the timer will begin counting down.\n"
        "Make sure you have enough time to complete the exam.",
        button_text);

    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(current_room_detail->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s", message);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        room_detail_controller_on_start_exam(current_room_detail->room_id);
    }
}

static void on_back_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;

    room_detail_controller_on_back();
}

void room_detail_controller_on_start_exam(const char* room_id)
{
    int sock = ui_get_socket();
    if (sock < 0)
        return;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_JOIN_ROOM);
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "room_id", room_id);
    cJSON_AddItemToObject(req, JSON_KEY_DATA, data);

    send_packet(sock, MSG_TYPE_REQ, req);
    cJSON_Delete(req);

    // Response will be handled in ui.c and will transition to exam window
}

void room_detail_controller_on_back()
{
    if (current_room_detail) {
        free(current_room_detail);
        current_room_detail = NULL;
    }

    ui_show_home(ui_get_username());
}
