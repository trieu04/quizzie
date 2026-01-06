#include "ui.h"
#include "net.h"

int main(int argc, char *argv[]) {
    ui_init(&argc, &argv);
    ui_show_login();
    gtk_main();
    return 0;
}
