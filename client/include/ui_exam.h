#ifndef UI_EXAM_H
#define UI_EXAM_H

#include "cJSON.h"
#include <gtk/gtk.h>

typedef struct
{
    char room_id[32];
    cJSON* questions;
    int* user_answers;
    time_t start_time;
    int duration_seconds;
    guint timer_id;
    GtkWidget* window;
    GtkWidget* timer_label;
    GtkWidget* questions_container;
} ExamState;

void ui_show_exam_window(GtkWidget** window_out, const char* room_id, cJSON* questions, int* answers, long start_time, int duration_minutes);
void exam_controller_on_submit();
void exam_controller_on_leave();

#endif
