#ifndef EXAM_HANDLER_H
#define EXAM_HANDLER_H

#include "cJSON.h"

void handle_join_room(int client_idx, cJSON* data);
void handle_submit_answer(int client_idx, cJSON* data);
void handle_finish_exam(int client_idx, cJSON* data);
void handle_get_exam_state(int client_idx, cJSON* data);

#endif
