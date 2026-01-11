#include "auth_handler.h"
#include "client_manager.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>

void handle_login(int client_idx, cJSON* data)
{
    cJSON* user_item = cJSON_GetObjectItem(data, "username");
    cJSON* pass_item = cJSON_GetObjectItem(data, "password");

    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item)) {
        client_send_error(client_idx, "Invalid format");
        return;
    }

    char* username = user_item->valuestring;
    char* password = pass_item->valuestring;

    // Check concurrent login
    client_kick_duplicate_user(username, client_idx);

    if (storage_check_credentials(username, password)) {
        client_set_logged_in(client_idx, username);

        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "status", "SUCCESS");
        cJSON_AddStringToObject(resp, "message", "Login successful");

        cJSON* data_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(data_obj, "role", storage_get_role(username));
        cJSON_AddItemToObject(resp, "data", data_obj);

        client_send_response(client_idx, "RES", resp);
        cJSON_Delete(resp);

        printf("User %s logged in as %s\n", username, storage_get_role(username));
    } else {
        client_send_error(client_idx, "Invalid credentials");
    }
}

void handle_register(int client_idx, cJSON* data)
{
    cJSON* user_item = cJSON_GetObjectItem(data, "username");
    cJSON* pass_item = cJSON_GetObjectItem(data, "password");

    if (!cJSON_IsString(user_item) || !cJSON_IsString(pass_item)) {
        client_send_error(client_idx, "Invalid format");
        return;
    }

    char* username = user_item->valuestring;
    char* password = pass_item->valuestring;

    int res = storage_add_user(username, password, NULL);
    if (res == 0) {
        client_send_success(client_idx, "Register successful");
        printf("User %s registered\n", username);
    } else if (res == -2) {
        client_send_error(client_idx, "Username already exists");
    } else {
        client_send_error(client_idx, "Registration failed");
    }
}

void handle_logout(int client_idx)
{
    client_logout(client_idx);
}
