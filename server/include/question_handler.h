#ifndef QUESTION_HANDLER_H
#define QUESTION_HANDLER_H

#include "cJSON.h"

void handle_import_questions(int client_idx, cJSON* data);
void handle_list_question_banks(int client_idx);
void handle_get_question_bank(int client_idx, cJSON* data);
void handle_update_question_bank(int client_idx, cJSON* data);
void handle_delete_question_bank(int client_idx, cJSON* data);

#endif
