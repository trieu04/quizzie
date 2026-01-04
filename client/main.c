#include "client.h"
#include "ui.h"
#include "net.h"

int main(int argc, char **argv) {
    ClientContext* ctx = client_init();
    if (!ctx) {
        fprintf(stderr, "Failed to initialize client context\n");
        return EXIT_FAILURE;
    }

    // Initialize UI with GTK
    ui_init(&argc, &argv);

    // Initialize client state
    client_run(ctx);

    // Run UI (GTK main loop)
    ui_run(ctx);

    // Cleanup
    ui_cleanup();
    client_cleanup(ctx);

    return EXIT_SUCCESS;
}
