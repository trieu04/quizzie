#include "ui_home.h"
#include "net.h"
#include "protocol.h"
#include "ui.h"
#include "window_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GtkListStore* room_store;
static GtkWidget* tree_view;

static void on_logout_btn_clicked(GtkWidget* widget, gpointer data);
static void on_refresh_clicked(GtkWidget* widget, gpointer data);
static void on_join_room_clicked(GtkWidget* widget, gpointer data);

static void on_logout_btn_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;
    home_controller_on_logout();
}

static void on_refresh_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;
    (void)data;

    int sock = ui_get_socket();
    if (sock < 0)
        return;

    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, JSON_KEY_ACTION, ACTION_LIST_ROOMS);
    send_packet(sock, MSG_TYPE_REQ, req);
    cJSON_Delete(req);
}

static void on_join_room_clicked(GtkWidget* widget, gpointer data)
{
    (void)widget;

    GtkTreeView* view = GTK_TREE_VIEW(data);
    GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
    GtkTreeModel* model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* room_id;
        gtk_tree_model_get(model, &iter, 0, &room_id, -1);

        // Transition to room detail screen
        ui_show_room_detail(room_id);

        g_free(room_id);
    }
}

void ui_show_home_window(GtkWidget** window_out, GtkWidget** status_label_out, const char* username)
{
    char title[64];
    snprintf(title, sizeof(title), "Quizzie Home - %s", username);
    GtkWidget* window = create_window(title, 900, 600);
    *window_out = window;

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Header
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    char welcome_msg[64];
    snprintf(welcome_msg, sizeof(welcome_msg), "Welcome, %s!", username);
    GtkWidget* lbl_welcome = gtk_label_new(welcome_msg);
    PangoAttrList* attrlist = pango_attr_list_new();
    PangoAttribute* attr = pango_attr_scale_new(1.5);
    pango_attr_list_insert(attrlist, attr);
    gtk_label_set_attributes(GTK_LABEL(lbl_welcome), attrlist);
    pango_attr_list_unref(attrlist);

    GtkWidget* btn_logout = gtk_button_new_with_label("Logout");
    g_signal_connect(btn_logout, "clicked", G_CALLBACK(on_logout_btn_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(header_box), lbl_welcome, FALSE, FALSE, 10);
    gtk_box_pack_end(GTK_BOX(header_box), btn_logout, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(vbox), header_box, FALSE, FALSE, 10);

    // Room List Title
    GtkWidget* title_label = gtk_label_new("Available Quiz Rooms");
    PangoAttrList* title_attrlist = pango_attr_list_new();
    PangoAttribute* title_attr = pango_attr_scale_new(1.2);
    pango_attr_list_insert(title_attrlist, title_attr);
    gtk_label_set_attributes(GTK_LABEL(title_label), title_attrlist);
    pango_attr_list_unref(title_attrlist);
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 5);

    // Room List Table
    room_store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(room_store));

    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "ID", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Name", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Status", renderer, "text", 2, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Open Time", renderer, "text", 3, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree_view), -1, "Duration (mins)", renderer, "text", 4, NULL);

    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled_window, -1, 300);
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 5);

    // Action buttons
    GtkWidget* action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* refresh_btn = gtk_button_new_with_label("Refresh List");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(action_box), refresh_btn, FALSE, FALSE, 10);

    GtkWidget* join_btn = gtk_button_new_with_label("View Room Details");
    g_signal_connect(join_btn, "clicked", G_CALLBACK(on_join_room_clicked), tree_view);
    gtk_box_pack_start(GTK_BOX(action_box), join_btn, FALSE, FALSE, 10);

    gtk_box_pack_start(GTK_BOX(vbox), action_box, FALSE, FALSE, 10);

    *status_label_out = gtk_label_new("Status: Connected");
    gtk_box_pack_start(GTK_BOX(vbox), *status_label_out, FALSE, FALSE, 0);

    gtk_widget_show_all(window);

    // Request room list
    on_refresh_clicked(NULL, NULL);
}

void home_update_room_list(cJSON* rooms_array)
{
    if (!room_store)
        return;

    gtk_list_store_clear(room_store);

    if (!rooms_array || !cJSON_IsArray(rooms_array))
        return;

    cJSON* room = NULL;
    cJSON_ArrayForEach(room, rooms_array)
    {
        cJSON* id = cJSON_GetObjectItem(room, "id");
        cJSON* name = cJSON_GetObjectItem(room, "name");
        cJSON* start_time = cJSON_GetObjectItem(room, "start_time");
        cJSON* end_time = cJSON_GetObjectItem(room, "end_time");
        cJSON* duration = cJSON_GetObjectItem(room, "duration");

        // Calculate status based on current time
        time_t now = time(NULL);
        time_t start = start_time && cJSON_IsNumber(start_time) ? (time_t)start_time->valuedouble : 0;
        time_t end = end_time && cJSON_IsNumber(end_time) ? (time_t)end_time->valuedouble : 0;

        const char* status;
        if (now < start) {
            status = "WAITING";
        } else if (now > end) {
            status = "CLOSED";
        } else {
            status = "OPEN";
        }

        char start_time_str[64] = "N/A";
        if (start > 0) {
            struct tm* tm_info = localtime(&start);
            strftime(start_time_str, sizeof(start_time_str), "%Y-%m-%d %H:%M", tm_info);
        }

        char duration_str[16] = "N/A";
        if (duration && cJSON_IsNumber(duration)) {
            snprintf(duration_str, sizeof(duration_str), "%d", duration->valueint);
        }

        gtk_list_store_insert_with_values(room_store, NULL, -1,
            0, id && cJSON_IsString(id) ? id->valuestring : "",
            1, name && cJSON_IsString(name) ? name->valuestring : "",
            2, status,
            3, start_time_str,
            4, duration_str,
            -1);
    }
}

