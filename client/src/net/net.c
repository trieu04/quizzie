#include "net.h"
#include <stdio.h>

void net_connect(const char *host, int port) {
    (void)host;
    (void)port;
    printf("Connecting to %s:%d\n", host, port);
}
