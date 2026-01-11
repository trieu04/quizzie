#include "question_handler.h"
#include "client_manager.h"
#include "storage.h"
#include <stdio.h>

void handle_import_questions(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_name = cJSON_GetObjectItem(data, "bank_name");
    cJSON* questions = cJSON_GetObjectItem(data, "questions");

    if (!cJSON_IsString(bank_name) || !cJSON_IsArray(questions)) {
        client_send_error(client_idx, "Invalid question data");
        return;
    }

    if (storage_save_question_bank(bank_name->valuestring, questions) == 0) {
        client_send_success(client_idx, "Questions imported");
    } else {
        client_send_error(client_idx, "Failed to import questions");
    }
}

void handle_list_question_banks(int client_idx)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* banks = cJSON_CreateArray();
    if (storage_list_question_banks(banks) == 0) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "status", "SUCCESS");
        cJSON_AddItemToObject(resp, "data", banks);
        client_send_response(client_idx, "RES", resp);
        cJSON_Delete(resp);
    } else {
        cJSON_Delete(banks);
        client_send_error(client_idx, "Failed to list question banks");
    }
}

void handle_get_question_bank(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_id = cJSON_GetObjectItem(data, "bank_id");
    if (!cJSON_IsString(bank_id)) {
        client_send_error(client_idx, "Invalid bank id");
        return;
    }

    cJSON* questions = NULL;
    if (storage_get_question_bank(bank_id->valuestring, &questions) == 0) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "status", "SUCCESS");
        cJSON_AddItemToObject(resp, "data", questions);
        client_send_response(client_idx, "RES", resp);
        cJSON_Delete(resp);
    } else {
        client_send_error(client_idx, "Bank not found");
    }
}

void handle_update_question_bank(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_id = cJSON_GetObjectItem(data, "bank_id");
    cJSON* questions = cJSON_GetObjectItem(data, "questions");

    if (!cJSON_IsString(bank_id) || !cJSON_IsArray(questions)) {
        client_send_error(client_idx, "Invalid data");
        return;
    }

    if (storage_update_question_bank(bank_id->valuestring, questions) == 0) {
        client_send_success(client_idx, "Question bank updated");
    } else {
        client_send_error(client_idx, "Failed to update bank");
    }
}

void handle_delete_question_bank(int client_idx, cJSON* data)
{
    if (!client_check_admin(client_idx)) {
        client_send_error(client_idx, "Permission denied");
        return;
    }

    cJSON* bank_id = cJSON_GetObjectItem(data, "bank_id");
    if (!cJSON_IsString(bank_id)) {
        client_send_error(client_idx, "Invalid bank id");
        return;
    }

    if (storage_delete_question_bank(bank_id->valuestring) == 0) {
        client_send_success(client_idx, "Question bank deleted");
    } else {
        client_send_error(client_idx, "Failed to delete bank");
    }
}
