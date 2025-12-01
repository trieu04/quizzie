#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include <sys/epoll.h>

// Structs
typedef struct {
    int sock;
    char username[50];
} Client;

typedef struct {
    int id;
    int host_sock;
    Client *clients[MAX_CLIENTS];
    int client_count;
    char questions[BUFFER_SIZE];
    char correct_answers[50];
} Room;

typedef struct {
    char type[20];
    char data[1000];
} Message;

typedef struct ServerContext {
    int server_fd;
    int epoll_fd;
    Client clients[MAX_CLIENTS];
    Room rooms[MAX_ROOMS];
    int client_count;
    int room_count;
    bool running;
} ServerContext;

// Function prototypes
ServerContext* server_init();
void server_cleanup(ServerContext* ctx);
void server_run(ServerContext* ctx);

#endif // SERVER_H