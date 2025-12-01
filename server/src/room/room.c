#include "../../include/room.h"
#include "../../include/storage.h"
#include "../../include/net.h"

int room_create(ServerContext* ctx, int host_sock) {
    if (ctx->room_count >= MAX_ROOMS) return -1;
    Room* room = &ctx->rooms[ctx->room_count];
    room->id = ctx->room_count + 1;
    room->host_sock = host_sock;
    room->client_count = 1;
    room->clients[0] = &ctx->clients[ctx->client_count - 1];
    storage_load_questions("../data/questions.txt", room->questions, room->correct_answers);

    char response[50];
    sprintf(response, "ROOM_CREATED:%d", room->id);
    net_send_to_client(host_sock, response, strlen(response));
    ctx->room_count++;
    return 0;
}

int room_join(ServerContext* ctx, int client_sock, int room_id) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].id == room_id) {
            ctx->rooms[i].clients[ctx->rooms[i].client_count] = &ctx->clients[ctx->client_count - 1];
            ctx->rooms[i].client_count++;
            net_send_to_client(client_sock, "JOINED", 6);
            return 0;
        }
    }
    return -1;
}

int room_start_quiz(ServerContext* ctx, int host_sock) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].host_sock == host_sock) {
            for (int j = 0; j < ctx->rooms[i].client_count; j++) {
                net_send_to_client(ctx->rooms[i].clients[j]->sock, ctx->rooms[i].questions, strlen(ctx->rooms[i].questions));
            }
            return 0;
        }
    }
    return -1;
}

int room_submit_answers(ServerContext* ctx, int client_sock, const char* answers) {
    for (int i = 0; i < ctx->room_count; i++) {
        for (int j = 0; j < ctx->rooms[i].client_count; j++) {
            if (ctx->rooms[i].clients[j]->sock == client_sock) {
                int score = (strcmp(answers, ctx->rooms[i].correct_answers) == 0) ? 5 : 0;
                char result[50];
                sprintf(result, "RESULT:%d/5", score);
                net_send_to_client(client_sock, result, strlen(result));
                return 0;
            }
        }
    }
    return -1;
}