#include "net.h"
#include <arpa/inet.h>

int net_connect(ClientContext* ctx, const char* ip, int port) {
    if (!ctx) return -1;

    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->socket_fd < 0) {
        perror("socket");
        return -1;
    }

    ctx->server_addr.sin_family = AF_INET;
    ctx->server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &ctx->server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        return -1;
    }

    if (connect(ctx->socket_fd, (struct sockaddr*)&ctx->server_addr, sizeof(ctx->server_addr)) < 0) {
        perror("connect");
        return -1;
    }

    return 0;
}

int net_send(ClientContext* ctx, const char* data, size_t len) {
    if (!ctx || ctx->socket_fd == -1) return -1;
    return send(ctx->socket_fd, data, len, 0);
}

int net_receive(ClientContext* ctx, char* buffer, size_t len) {
    if (!ctx || ctx->socket_fd == -1) return -1;
    return recv(ctx->socket_fd, buffer, len, 0);
}

void net_close(ClientContext* ctx) {
    if (ctx && ctx->socket_fd != -1) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
}
