#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include "cJSON.h"

void handle_login(int client_idx, cJSON* data);
void handle_register(int client_idx, cJSON* data);
void handle_logout(int client_idx);

#endif
