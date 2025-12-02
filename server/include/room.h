#ifndef ROOM_H
#define ROOM_H

#include "server.h"

// Function prototypes
int room_create(ServerContext* ctx, int host_sock, const char* username, const char* config);
int room_join(ServerContext* ctx, int client_sock, int room_id, const char* username);
int room_start_quiz(ServerContext* ctx, int host_sock);
int room_submit_answers(ServerContext* ctx, int client_sock, const char* answers);
int room_get_stats(ServerContext* ctx, int host_sock);
int room_client_start_quiz(ServerContext* ctx, int client_sock);
Room* room_find_by_host_username(ServerContext* ctx, const char* username);
void room_list(ServerContext* ctx, int client_sock);
int room_rejoin_as_host(ServerContext* ctx, int client_sock, const char* username);
int room_set_config(ServerContext* ctx, int host_sock, const char* config);

#endif // ROOM_H