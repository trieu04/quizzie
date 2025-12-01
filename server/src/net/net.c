#include "../../include/net.h"
#include "../../include/server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

int net_setup(ServerContext* ctx) {
    ctx->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_fd < 0) return -1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(ctx->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (listen(ctx->server_fd, 5) < 0) return -1;

    // Set non-blocking
    int flags = fcntl(ctx->server_fd, F_GETFL, 0);
    fcntl(ctx->server_fd, F_SETFL, flags | O_NONBLOCK);

    // Setup epoll
    ctx->epoll_fd = epoll_create1(0);
    struct epoll_event event = {0};
    event.events = EPOLLIN;
    event.data.fd = ctx->server_fd;
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->server_fd, &event);

    return 0;
}

int net_accept_client(ServerContext* ctx) {
    int client_sock = accept(ctx->server_fd, NULL, NULL);
    if (client_sock >= 0) {
        int flags = fcntl(client_sock, F_GETFL, 0);
        fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);

        struct epoll_event event = {0};
        event.events = EPOLLIN;
        event.data.fd = client_sock;
        epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, client_sock, &event);

        ctx->clients[ctx->client_count].sock = client_sock;
        strcpy(ctx->clients[ctx->client_count].username, "User");
        ctx->client_count++;
        LOG_INFO("New client connected");
    }
    return client_sock;
}

int net_send_to_client(int sock, const char* data, size_t len) {
    return send(sock, data, len, 0);
}

int net_receive_from_client(int sock, char* buffer, size_t len) {
    return recv(sock, buffer, len, 0);
}

void net_close_client(ServerContext* ctx, int sock) {
    epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, sock, NULL);
    close(sock);
    // Remove from clients array if needed
}