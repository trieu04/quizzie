#include "client.h"
#include "ui.h"
#include "net.h"

int main() {
    ClientContext* ctx = client_init();
    if (!ctx) {
        LOG_ERROR("Failed to initialize client context");
        return EXIT_FAILURE;
    }

    // Initialize UI
    ui_init();

    // Connect to server (optional at startup, can be done later)
    // For now, we just log it to UI
    ui_show_message("Welcome to Quizzie Client!");

    // Main loop
    client_run(ctx);

    // Cleanup
    ui_cleanup();
    client_cleanup(ctx);

    return EXIT_SUCCESS;
}
