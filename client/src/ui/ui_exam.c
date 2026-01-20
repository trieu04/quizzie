#include "ui_exam.h"
#include "net.h"
#include "protocol.h"
#include "ui.h"
#include "window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static ExamState* current_exam = NULL;

// Forward declarations
static gboolean on_timer_tick(gpointer data);
static void on_submit_btn_clicked(GtkWidget* widget, gpointer data);
static void on_leave_btn_clicked(GtkWidget* widget, gpointer data);
static void on_radio_toggled(GtkToggleButton* button, gpointer data);
static void submit_answer_to_server(int question_index, int answer_index);
static void render_all_questions();

void ui_show_exam_window(GtkWidget** window_out, const char* room_id, cJSON* questions, int* answers, long start_time, int duration_minutes)
{
    GtkWidget* window = create_window("Bài thi", 900, 700);
    *window_out = window;

    // Initialize exam state
    if (current_exam) {
        if (current_exam->timer_id > 0) {
            g_source_remove(current_exam->timer_id);
        }
        if (current_exam->questions) {
            cJSON_Delete(current_exam->questions);
        }
        if (current_exam->user_answers) {
            free(current_exam->user_answers);
        }
        free(current_exam);
    }

    current_exam = malloc(sizeof(ExamState));
    memset(current_exam, 0, sizeof(ExamState));
    strncpy(current_exam->room_id, room_id, sizeof(current_exam->room_id) - 1);
    current_exam->questions = cJSON_Duplicate(questions, 1);
    current_exam->start_time = start_time;
    current_exam->duration_seconds = duration_minutes * 60;
    current_exam->window = window;

    int num_questions = cJSON_GetArraySize(questions);
    current_exam->user_answers = malloc(sizeof(int) * num_questions);
    for (int i = 0; i < num_questions; i++) {
        current_exam->user_answers[i] = answers[i];
    }

    // Main layout
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Top bar with timer
    GtkWidget* top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    current_exam->timer_label = gtk_label_new("Thời gian còn lại: --:--");
    PangoAttrList* timer_attrlist = pango_attr_list_new();
    PangoAttribute* timer_attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(timer_attrlist, timer_attr);
    gtk_label_set_attributes(GTK_LABEL(current_exam->timer_label), timer_attrlist);
    pango_attr_list_unref(timer_attrlist);
    gtk_box_pack_start(GTK_BOX(top_bar), current_exam->timer_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), top_bar, FALSE, FALSE, 0);

    // Instructions
    GtkWidget* instructions = gtk_label_new("Chọn đáp án cho các câu hỏi. Đáp án sẽ tự động lưu.");
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), instructions, FALSE, FALSE, 0);

    // Scrollable questions container
    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 450);

    current_exam->questions_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_set_border_width(GTK_CONTAINER(current_exam->questions_container), 10);
    gtk_container_add(GTK_CONTAINER(scrolled_window), current_exam->questions_container);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    // Action buttons
    GtkWidget* btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* btn_leave = gtk_button_new_with_label("Thoát");
    GtkWidget* btn_submit = gtk_button_new_with_label("Nộp bài");

    g_signal_connect(btn_leave, "clicked", G_CALLBACK(on_leave_btn_clicked), NULL);
    g_signal_connect(btn_submit, "clicked", G_CALLBACK(on_submit_btn_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(btn_box), btn_leave, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_submit, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 10);

    // Render all questions
    render_all_questions();

    // Start timer
    current_exam->timer_id = g_timeout_add(1000, on_timer_tick, NULL);

    gtk_widget_show_all(window);
}

static void render_all_questions()
{
    if (!current_exam)
        return;

    int num_questions = cJSON_GetArraySize(current_exam->questions);

    for (int q_idx = 0; q_idx < num_questions; q_idx++) {
        cJSON* question = cJSON_GetArrayItem(current_exam->questions, q_idx);
        cJSON* text = cJSON_GetObjectItem(question, "question");
        cJSON* options = cJSON_GetObjectItem(question, "options");

        // Question container
        GtkWidget* question_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width(GTK_CONTAINER(question_box), 10);

        // Question number and text
        char question_header[1024];
        snprintf(question_header, sizeof(question_header), "Câu %d: %s",
            q_idx + 1,
            text && cJSON_IsString(text) ? text->valuestring : "N/A");

        GtkWidget* question_label = gtk_label_new(question_header);
        gtk_label_set_line_wrap(GTK_LABEL(question_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(question_label), 0.0);
        PangoAttrList* attrlist = pango_attr_list_new();
        PangoAttribute* attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        pango_attr_list_insert(attrlist, attr);
        gtk_label_set_attributes(GTK_LABEL(question_label), attrlist);
        pango_attr_list_unref(attrlist);
        gtk_box_pack_start(GTK_BOX(question_box), question_label, FALSE, FALSE, 0);

        // Radio buttons for options
        if (options && cJSON_IsArray(options)) {
            int num_options = cJSON_GetArraySize(options);
            int current_answer = current_exam->user_answers[q_idx];

            // Create a hidden dummy radio button to allow "no selection" state
            // GTK radio groups require at least one button to be active
            GtkWidget* dummy_radio = gtk_radio_button_new(NULL);
            gtk_widget_set_no_show_all(dummy_radio, TRUE);
            gtk_box_pack_start(GTK_BOX(question_box), dummy_radio, FALSE, FALSE, 0);
            GSList* radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dummy_radio));

            for (int opt_idx = 0; opt_idx < num_options; opt_idx++) {
                cJSON* opt = cJSON_GetArrayItem(options, opt_idx);
                if (opt && cJSON_IsString(opt)) {
                    char label[512];
                    snprintf(label, sizeof(label), "%c. %s", 'A' + opt_idx, opt->valuestring);

                    GtkWidget* radio_btn = gtk_radio_button_new_with_label(radio_group, label);
                    radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_btn));

                    // Set active if this was previously answered
                    if (current_answer == opt_idx) {
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_btn), TRUE);
                    }

                    // Store question index and option index as data
                    int packed_data = (q_idx << 16) | opt_idx;
                    g_signal_connect(radio_btn, "toggled", G_CALLBACK(on_radio_toggled), GINT_TO_POINTER(packed_data));

                    gtk_box_pack_start(GTK_BOX(question_box), radio_btn, FALSE, FALSE, 0);
                }
            }
        }

        // Add separator
        GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(question_box), separator, FALSE, FALSE, 5);

        gtk_box_pack_start(GTK_BOX(current_exam->questions_container), question_box, FALSE, FALSE, 0);
    }
}

static gboolean on_timer_tick(gpointer data)
{
    (void)data;

    if (!current_exam)
        return FALSE;

    time_t now = time(NULL);
    int elapsed = (int)(now - current_exam->start_time);
    int remaining = current_exam->duration_seconds - elapsed;

    if (remaining <= 0) {
        gtk_label_set_text(GTK_LABEL(current_exam->timer_label), "Hết giờ!");
        // Auto-submit
        exam_controller_on_submit();
        return FALSE;
    }

    int minutes = remaining / 60;
    int seconds = remaining % 60;
    char timer_text[64];
    snprintf(timer_text, sizeof(timer_text), "Thời gian còn lại: %02d:%02d", minutes, seconds);
    gtk_label_set_text(GTK_LABEL(current_exam->timer_label), timer_text);

    return TRUE;
}

static void on_submit_btn_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;

    if (!current_exam)
        return;

    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(current_exam->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Bạn có chắc muốn nộp bài?\n\nBạn sẽ không thể thay đổi đáp án sau khi nộp.");

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        exam_controller_on_submit();
    }
}

static void on_leave_btn_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;

    if (!current_exam)
        return;

    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(current_exam->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Bạn có chắc muốn thoát?\n\nĐáp án đã được lưu và bạn có thể quay lại sau.");

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        exam_controller_on_leave();
    }
}

static void on_radio_toggled(GtkToggleButton* button, gpointer data)
{
    if (!current_exam)
        return;

    if (!gtk_toggle_button_get_active(button))
        return;

    // Unpack question index and option index
    int packed_data = GPOINTER_TO_INT(data);
    int question_index = packed_data >> 16;
    int answer_index = packed_data & 0xFFFF;

    current_exam->user_answers[question_index] = answer_index;

    // Submit to server
    submit_answer_to_server(question_index, answer_index);
}

static void submit_answer_to_server(int question_index, int answer_index)
{
    if (!current_exam)
        return;

    int sock = ui_get_socket();
    if (sock < 0)
        return;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_SUBMIT_ANSWER);
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "room_id", current_exam->room_id);
    cJSON_AddNumberToObject(data, "question_index", question_index);
    cJSON_AddNumberToObject(data, "answer_index", answer_index);
    cJSON_AddItemToObject(req, JSON_KEY_DATA, data);

    send_packet(sock, MSG_TYPE_REQ, req);
    cJSON_Delete(req);
}

void exam_controller_on_submit()
{
    if (!current_exam)
        return;

    // Stop timer
    if (current_exam->timer_id > 0) {
        g_source_remove(current_exam->timer_id);
        current_exam->timer_id = 0;
    }

    int sock = ui_get_socket();
    if (sock < 0)
        return;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_FINISH_EXAM);
    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "room_id", current_exam->room_id);
    cJSON_AddItemToObject(req, JSON_KEY_DATA, data);

    send_packet(sock, MSG_TYPE_REQ, req);
    cJSON_Delete(req);

    // Clean up will happen after receiving result
}

void exam_controller_on_leave()
{
    if (current_exam) {
        if (current_exam->timer_id > 0) {
            g_source_remove(current_exam->timer_id);
        }
        if (current_exam->questions) {
            cJSON_Delete(current_exam->questions);
        }
        if (current_exam->user_answers) {
            free(current_exam->user_answers);
        }
        free(current_exam);
        current_exam = NULL;
    }

    // Return to home
    ui_show_home(ui_get_username());
}
