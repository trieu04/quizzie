#include "../../include/room.h"
#include "../../include/storage.h"
#include "../../include/net.h"

// Helper function to find a client by socket in a room
static Client* find_client_in_room(Room* room, int sock) {
    for (int i = 0; i < room->client_count; i++) {
        if (room->clients[i] && room->clients[i]->sock == sock) {
            return room->clients[i];
        }
    }
    return NULL;
}

// Helper function to find client by socket in server context
static Client* find_client(ServerContext* ctx, int sock) {
    for (int i = 0; i < ctx->client_count; i++) {
        if (ctx->clients[i].sock == sock) {
            return &ctx->clients[i];
        }
    }
    return NULL;
}

int room_create(ServerContext* ctx, int host_sock) {
    if (ctx->room_count >= MAX_ROOMS) {
        net_send_to_client(host_sock, "ERROR:Max rooms reached", 23);
        return -1;
    }

    Room* room = &ctx->rooms[ctx->room_count];
    room->id = ctx->room_count + 1;
    room->host_sock = host_sock;
    room->client_count = 1;

    // Find the client and add to room
    Client* client = find_client(ctx, host_sock);
    if (client) {
        room->clients[0] = client;
    }

    // Try multiple paths for questions file
    const char* candidates[] = {
        "data/questions.txt",
        "../data/questions.txt",
        "../../data/questions.txt",
        NULL
    };

    int loaded = 0;
    for (int i = 0; candidates[i] != NULL; ++i) {
        if (storage_load_questions(candidates[i], room->questions, room->correct_answers) == 0) {
            loaded = 1;
            break;
        }
    }

    if (!loaded) {
        LOG_ERROR("Failed to load questions for room");
        net_send_to_client(host_sock, "ERROR:Failed to load questions", 30);
        return -1;
    }

    char response[50];
    sprintf(response, "ROOM_CREATED:%d", room->id);
    net_send_to_client(host_sock, response, strlen(response));
    ctx->room_count++;
    LOG_INFO("Room created successfully");
    return 0;
}

int room_join(ServerContext* ctx, int client_sock, int room_id) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].id == room_id) {
            if (ctx->rooms[i].client_count >= MAX_CLIENTS) {
                net_send_to_client(client_sock, "ERROR:Room is full", 18);
                return -1;
            }

            Client* client = find_client(ctx, client_sock);
            if (client) {
                ctx->rooms[i].clients[ctx->rooms[i].client_count] = client;
                ctx->rooms[i].client_count++;

                char response[50];
                sprintf(response, "JOINED:%d", room_id);
                net_send_to_client(client_sock, response, strlen(response));
                LOG_INFO("Client joined room");
                return 0;
            }
        }
    }
    net_send_to_client(client_sock, "ERROR:Room not found", 20);
    return -1;
}

int room_start_quiz(ServerContext* ctx, int host_sock) {
    for (int i = 0; i < ctx->room_count; i++) {
        if (ctx->rooms[i].host_sock == host_sock) {
            LOG_INFO("Starting quiz for room");
            for (int j = 0; j < ctx->rooms[i].client_count; j++) {
                if (ctx->rooms[i].clients[j]) {
                    net_send_to_client(ctx->rooms[i].clients[j]->sock,
                        ctx->rooms[i].questions,
                        strlen(ctx->rooms[i].questions));
                }
            }
            return 0;
        }
    }
    net_send_to_client(host_sock, "ERROR:Not a room host", 21);
    return -1;
}

int room_submit_answers(ServerContext* ctx, int client_sock, const char* answers) {
    for (int i = 0; i < ctx->room_count; i++) {
        Room* room = &ctx->rooms[i];
        Client* client = find_client_in_room(room, client_sock);

        if (client) {
            // Calculate score by comparing each answer
            int score = 0;
            int total = 0;

            const char* correct = room->correct_answers;
            const char* submitted = answers;

            // Count total questions from correct answers (comma-separated)
            for (const char* p = correct; *p; p++) {
                if (*p == ',' || *(p + 1) == '\0') total++;
            }
            if (total == 0) total = 1;

            // Compare answers
            int idx = 0;
            char correct_copy[256];
            strncpy(correct_copy, correct, sizeof(correct_copy) - 1);
            correct_copy[sizeof(correct_copy) - 1] = '\0';

            char* token = strtok(correct_copy, ",");
            while (token && idx < (int)strlen(submitted)) {
                if (submitted[idx] == token[0]) {
                    score++;
                }
                idx++;
                token = strtok(NULL, ",");
            }

            char result[50];
            sprintf(result, "RESULT:%d/%d", score, total);
            net_send_to_client(client_sock, result, strlen(result));
            LOG_INFO("Answers submitted and scored");
            return 0;
        }
    }
    net_send_to_client(client_sock, "ERROR:Not in any room", 21);
    return -1;
}