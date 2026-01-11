#include "client_manager.h"
#include "net.h"
#include "server.h"
#include "storage.h"
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define POLL_TIMEOUT_MS -1
#define DEFAULT_PORT 8080

int main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    storage_init();
    server_start(port);
    return 0;
}

void server_start(int port)
{
    int server_fd = net_listen(port);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to start server\n");
        exit(1);
    }

    client_manager_init();

    // Setup poll server fd
    struct pollfd* fds = client_get_pollfds();
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Server loop started on port %d...\n", port);
    fflush(stdout);

    while (1) {
        int ret = poll(fds, MAX_CLIENTS + 1, POLL_TIMEOUT_MS);
        if (ret < 0) {
            perror("poll");
            break;
        }

        // Check for new connection
        if (fds[0].revents & POLLIN) {
            client_add(server_fd);
        }

        // Check clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            ClientState* client = client_get_state(i);
            if (client->fd != -1 && (fds[i + 1].revents & POLLIN)) {
                client_handle_activity(i);
            }
        }
    }

    close(server_fd);
}
