#include "server.h"
#include "storage.h"

int main() {
    ServerContext* ctx = server_init();
    if (!ctx) {
        LOG_ERROR("Failed to initialize server context");
        return EXIT_FAILURE;
    }

    // Load preserved state if available
    storage_load_server_state(ctx);

    server_run(ctx);
    server_cleanup(ctx);
    return EXIT_SUCCESS;
}