#include "server.h"
#include "net.h"
#include "storage.h"

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    storage_init();
    server_start(port);
    return 0;
}

void server_start(int port) {
    printf("Server starting on port %d...\n", port);
    net_listen(port);
}
