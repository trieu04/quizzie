#include "server.h"


int main() {
    ServerContext* ctx = server_init();
    if (!ctx) {
        LOG_ERROR("Failed to initialize server context");
        return EXIT_FAILURE;
    }
    server_run(ctx);
    server_cleanup(ctx);
    return EXIT_SUCCESS;
}