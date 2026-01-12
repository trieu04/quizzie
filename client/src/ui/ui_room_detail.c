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
    GtkWidget* lblID;
    GtkWidget* lblName;
    GtkWidget* lblStatus;
    GtkWidget* lblStart;
    GtkWidget* lblEnd;
    GtkWidget* lblNumQ;
    GtkWidget* lblAttempts;
    GtkWidget* lblDuration;
    GtkWidget* results_scroll;
    GtkWidget* start_button;
    GtkWidget* view_button;
    GtkWidget* results_tree_view;
    int has_active_exam;
} RoomDetailState;

static RoomDetailState* current_room_detail = NULL;

static void on_start_exam_clicked(GtkWidget* widget, gpointer data);
static void on_back_clicked(GtkWidget* widget, gpointer data);
static void on_view_result_clicked(GtkWidget* widget, gpointer data);

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

    // Room info grid (same style as admin)
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 5);

    // Create value labels
    current_room_detail->lblID = gtk_label_new("...");
    current_room_detail->lblName = gtk_label_new("Loading...");
    current_room_detail->lblStatus = gtk_label_new("...");
    current_room_detail->lblStart = gtk_label_new("...");
    current_room_detail->lblEnd = gtk_label_new("...");
    current_room_detail->lblNumQ = gtk_label_new("...");
    current_room_detail->lblAttempts = gtk_label_new("...");
    current_room_detail->lblDuration = gtk_label_new("...");

    // Set alignment: values align left
    gtk_widget_set_halign(current_room_detail->lblID, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblName, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblStatus, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblStart, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblEnd, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblNumQ, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblAttempts, GTK_ALIGN_START);
    gtk_widget_set_halign(current_room_detail->lblDuration, GTK_ALIGN_START);

    int row = 0;
    GtkWidget* label;

    label = gtk_label_new("Room ID:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblID, 1, row++, 1, 1);

    label = gtk_label_new("Room Name:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblName, 1, row++, 1, 1);

    label = gtk_label_new("Status:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblStatus, 1, row++, 1, 1);

    label = gtk_label_new("Open Time:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblStart, 1, row++, 1, 1);

    label = gtk_label_new("Close Time:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblEnd, 1, row++, 1, 1);

    label = gtk_label_new("Num Questions:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblNumQ, 1, row++, 1, 1);

    label = gtk_label_new("Allowed Attempts:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblAttempts, 1, row++, 1, 1);

    label = gtk_label_new("Duration (mins):");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), current_room_detail->lblDuration, 1, row++, 1, 1);

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
    current_room_detail->view_button = gtk_button_new_with_label("Xem lại bài đã chọn");

    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_back_clicked), NULL);
    g_signal_connect(current_room_detail->start_button, "clicked", G_CALLBACK(on_start_exam_clicked), NULL);
    g_signal_connect(current_room_detail->view_button, "clicked", G_CALLBACK(on_view_result_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(btn_box), btn_back, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), current_room_detail->view_button, FALSE, FALSE, 0);
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
    cJSON* show_answers = cJSON_GetObjectItem(room_info, "show_answers");
    cJSON* results = cJSON_GetObjectItem(room_data, "results");
    cJSON* has_active_exam_obj = cJSON_GetObjectItem(room_data, "has_active_exam");

    int show_answers_enabled = show_answers && cJSON_IsNumber(show_answers) ? show_answers->valueint : 0;

    // Update individual labels (same style as admin)
    if (id && cJSON_IsString(id)) {
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblID), id->valuestring);
    }

    if (name && cJSON_IsString(name)) {
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblName), name->valuestring);
    }

    if (status && cJSON_IsString(status)) {
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblStatus), status->valuestring);
    }

    // Format and set time labels
    char buf[64];
    if (start_time && cJSON_IsNumber(start_time)) {
        time_t st = (time_t)start_time->valuedouble;
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&st));
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblStart), buf);
    }

    if (end_time && cJSON_IsNumber(end_time)) {
        time_t et = (time_t)end_time->valuedouble;
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&et));
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblEnd), buf);
    }

    if (num_questions && cJSON_IsNumber(num_questions)) {
        snprintf(buf, sizeof(buf), "%d", num_questions->valueint);
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblNumQ), buf);
    }

    if (allowed_attempts && cJSON_IsNumber(allowed_attempts)) {
        snprintf(buf, sizeof(buf), "%d", allowed_attempts->valueint);
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblAttempts), buf);
    }

    if (duration && cJSON_IsNumber(duration)) {
        snprintf(buf, sizeof(buf), "%d", duration->valueint);
        gtk_label_set_text(GTK_LABEL(current_room_detail->lblDuration), buf);
    }

    // Update button text based on exam state
    if (has_active_exam_obj && cJSON_IsBool(has_active_exam_obj) && cJSON_IsTrue(has_active_exam_obj)) {
        current_room_detail->has_active_exam = 1;
        gtk_button_set_label(GTK_BUTTON(current_room_detail->start_button), "Continue Exam");
    } else {
        current_room_detail->has_active_exam = 0;
        gtk_button_set_label(GTK_BUTTON(current_room_detail->start_button), "Start Quiz");
    }

    // Create results table with column for timestamp (hidden), show_answers flag, and view button
    GtkListStore* store = gtk_list_store_new(6, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_LONG, G_TYPE_INT);

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
                    4, (long)ts, // Store timestamp for later use
                    5, show_answers_enabled, // Store show_answers flag
                    -1);
                user_result_count++;
            }
        }
    }

    // Create tree view
    GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    current_room_detail->results_tree_view = tree_view;

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

static void show_exam_review_dialog(GtkWidget* parent, cJSON* review_data);

static void on_view_result_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;

    if (!current_room_detail || !current_room_detail->results_tree_view)
        return;

    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(current_room_detail->results_tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkWidget* msg = gtk_message_dialog_new(GTK_WINDOW(current_room_detail->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            "Vui lòng chọn một bài làm để xem lại.");
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        return;
    }

    long timestamp;
    int show_answers;
    gtk_tree_model_get(model, &iter, 4, &timestamp, 5, &show_answers, -1);

    if (timestamp == 0) {
        GtkWidget* msg = gtk_message_dialog_new(GTK_WINDOW(current_room_detail->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
            "Không có bài làm để xem.");
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        return;
    }

    if (!show_answers) {
        GtkWidget* msg = gtk_message_dialog_new(GTK_WINDOW(current_room_detail->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
            "Quản trị viên chưa cho phép xem đáp án cho phòng thi này.");
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        return;
    }

    // Request exam review from server
    int sock = ui_get_socket();
    if (sock < 0) {
        return;
    }

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "GET_EXAM_REVIEW");
    cJSON* data_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(data_obj, "room_id", current_room_detail->room_id);
    cJSON_AddNumberToObject(data_obj, "timestamp", timestamp);
    cJSON_AddItemToObject(req, "data", data_obj);

    send_packet(sock, "REQ", req);
    cJSON_Delete(req);

    // Wait for response (blocking for simplicity - in production use callbacks)
    char type[4];
    cJSON* resp = NULL;
    if (receive_packet(sock, type, &resp) == 0 && strcmp(type, "RES") == 0) {
        cJSON* status = cJSON_GetObjectItem(resp, "status");
        if (status && strcmp(status->valuestring, "SUCCESS") == 0) {
            cJSON* review_data = cJSON_GetObjectItem(resp, "data");
            if (review_data) {
                show_exam_review_dialog(current_room_detail->window, review_data);
            }
        } else {
            cJSON* message = cJSON_GetObjectItem(resp, "message");
            const char* msg_text = message && cJSON_IsString(message) ? message->valuestring : "Failed to retrieve exam review";
            GtkWidget* msg_dialog = gtk_message_dialog_new(GTK_WINDOW(current_room_detail->window),
                GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", msg_text);
            gtk_dialog_run(GTK_DIALOG(msg_dialog));
            gtk_widget_destroy(msg_dialog);
        }
        cJSON_Delete(resp);
    }
}

static void show_exam_review_dialog(GtkWidget* parent, cJSON* review_data)
{
    cJSON* questions = cJSON_GetObjectItem(review_data, "questions");
    cJSON* answers = cJSON_GetObjectItem(review_data, "answers");
    cJSON* score = cJSON_GetObjectItem(review_data, "score");
    cJSON* correct_count = cJSON_GetObjectItem(review_data, "correct_count");
    cJSON* num_questions = cJSON_GetObjectItem(review_data, "num_questions");

    if (!questions || !answers) {
        return;
    }

    GtkWidget* dialog = gtk_dialog_new_with_buttons("Xem lại bài làm",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Đóng", GTK_RESPONSE_CLOSE,
        NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 600);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    // Header with score
    char header_text[256];
    snprintf(header_text, sizeof(header_text),
        "Kết quả: %d/%d câu đúng - Điểm: %d",
        correct_count ? correct_count->valueint : 0,
        num_questions ? num_questions->valueint : 0,
        score ? score->valueint : 0);

    GtkWidget* header_label = gtk_label_new(header_text);
    PangoAttrList* attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attr_list, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(header_label), attr_list);
    pango_attr_list_unref(attr_list);
    gtk_box_pack_start(GTK_BOX(vbox), header_label, FALSE, FALSE, 5);

    // Separator
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // Scrolled window for questions
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    // Box to hold all questions
    GtkWidget* questions_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(questions_vbox), 10);
    gtk_container_add(GTK_CONTAINER(scrolled), questions_vbox);

    // Iterate through questions
    int q_count = cJSON_GetArraySize(questions);
    for (int i = 0; i < q_count; i++) {
        cJSON* question = cJSON_GetArrayItem(questions, i);
        cJSON* user_answer = cJSON_GetArrayItem(answers, i);

        if (!question) continue;

        cJSON* q_text = cJSON_GetObjectItem(question, "question");
        cJSON* options = cJSON_GetObjectItem(question, "options");
        cJSON* correct_answer = cJSON_GetObjectItem(question, "correct_answer");
        cJSON* correct_index = cJSON_GetObjectItem(question, "correct_index");

        int correct_idx = correct_index ? correct_index->valueint : 
                         (correct_answer ? correct_answer->valueint : -1);
        int user_idx = user_answer && cJSON_IsNumber(user_answer) ? (int)user_answer->valuedouble : -1;

        // Frame for each question
        char frame_title[32];
        snprintf(frame_title, sizeof(frame_title), "Câu %d", i + 1);
        GtkWidget* frame = gtk_frame_new(frame_title);
        gtk_box_pack_start(GTK_BOX(questions_vbox), frame, FALSE, FALSE, 0);

        GtkWidget* q_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_set_border_width(GTK_CONTAINER(q_vbox), 10);
        gtk_container_add(GTK_CONTAINER(frame), q_vbox);

        // Question text
        GtkWidget* q_label = gtk_label_new(q_text && cJSON_IsString(q_text) ? q_text->valuestring : "N/A");
        gtk_label_set_line_wrap(GTK_LABEL(q_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(q_label), 0.0);
        PangoAttrList* q_attr = pango_attr_list_new();
        pango_attr_list_insert(q_attr, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(q_label), q_attr);
        pango_attr_list_unref(q_attr);
        gtk_box_pack_start(GTK_BOX(q_vbox), q_label, FALSE, FALSE, 5);

        // Options
        if (options && cJSON_IsArray(options)) {
            int opt_count = cJSON_GetArraySize(options);
            for (int j = 0; j < opt_count; j++) {
                cJSON* opt = cJSON_GetArrayItem(options, j);
                if (!opt || !cJSON_IsString(opt)) continue;

                char opt_text[512];
                const char* status = "";
                if (j == correct_idx && j == user_idx) {
                    status = "✓ ĐÁP ÁN ĐÚNG (Bạn đã chọn)";
                } else if (j == correct_idx) {
                    status = "✓ ĐÁP ÁN ĐÚNG";
                } else if (j == user_idx) {
                    status = "✗ Bạn đã chọn (Sai)";
                }

                snprintf(opt_text, sizeof(opt_text), "%c) %s %s", 'A' + j, opt->valuestring, status);

                GtkWidget* opt_label = gtk_label_new(opt_text);
                gtk_label_set_line_wrap(GTK_LABEL(opt_label), TRUE);
                gtk_label_set_xalign(GTK_LABEL(opt_label), 0.0);

                // Color code
                if (j == correct_idx) {
                    // Green for correct answer
                    PangoAttrList* attr = pango_attr_list_new();
                    pango_attr_list_insert(attr, pango_attr_foreground_new(0, 32768, 0));
                    pango_attr_list_insert(attr, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
                    gtk_label_set_attributes(GTK_LABEL(opt_label), attr);
                    pango_attr_list_unref(attr);
                } else if (j == user_idx) {
                    // Red for wrong answer
                    PangoAttrList* attr = pango_attr_list_new();
                    pango_attr_list_insert(attr, pango_attr_foreground_new(50000, 0, 0));
                    gtk_label_set_attributes(GTK_LABEL(opt_label), attr);
                    pango_attr_list_unref(attr);
                }

                gtk_box_pack_start(GTK_BOX(q_vbox), opt_label, FALSE, FALSE, 2);
            }
        }
    }

    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
