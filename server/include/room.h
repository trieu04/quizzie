#ifndef ROOM_H
#define ROOM_H

#include "server.h"

// Function prototypes
int room_create(ServerContext* ctx, int host_sock);
int room_join(ServerContext* ctx, int client_sock, int room_id);
int room_start_quiz(ServerContext* ctx, int host_sock);
int room_submit_answers(ServerContext* ctx, int client_sock, const char* answers);

#endif // ROOM_H