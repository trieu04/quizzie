#include "include/server.h"
#include "include/storage.h"

int main() {
    ServerContext* ctx = server_init();
    if (!ctx) {
        LOG_ERROR("Failed to initialize server context");
        return EXIT_FAILURE;
    }

    // Load preserved state if available
    if (storage_load_server_state(ctx) == 0) {
        LOG_INFO("Server state restored from previous session");
    }

    server_run(ctx);
    server_cleanup(ctx);
    return EXIT_SUCCESS;
}