#ifndef ROOM_HANDLER_H
#define ROOM_HANDLER_H

#include "cJSON.h"

void handle_create_room(int client_idx, cJSON* data);
void handle_list_rooms(int client_idx);
void handle_get_room_stats(int client_idx, cJSON* data);
void handle_delete_room(int client_idx, cJSON* data);
void handle_close_room(int client_idx, cJSON* data);

#endif
