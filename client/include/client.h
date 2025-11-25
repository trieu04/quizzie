#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <netinet/in.h>

// Forward declarations
struct ClientContext;

typedef enum {
    PAGE_LOGIN,
    PAGE_DASHBOARD,
    PAGE_ROOM_LIST,
    PAGE_QUIZ
} AppState;

typedef struct ClientContext {
    int socket_fd;
    struct sockaddr_in server_addr;
    bool running;
    AppState current_state;
    // UI related state can be added here or in a separate struct
} ClientContext;

// Function prototypes
ClientContext* client_init();
void client_cleanup(ClientContext* ctx);
void client_run(ClientContext* ctx);

#endif // CLIENT_H
