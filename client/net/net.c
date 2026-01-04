#include "net.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

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
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }

    if (connect(ctx->socket_fd, (struct sockaddr*)&ctx->server_addr, sizeof(ctx->server_addr)) < 0) {
        perror("connect");
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        return -1;
    }

    // Set socket to non-blocking mode after connection
    int flags = fcntl(ctx->socket_fd, F_GETFL, 0);
    fcntl(ctx->socket_fd, F_SETFL, flags | O_NONBLOCK);

    ctx->connected = true;
    return 0;
}

int net_send(ClientContext* ctx, const char* data, size_t len) {
    if (!ctx || ctx->socket_fd == -1) return -1;
    
    // Add newline delimiter if not present
    char buffer[BUFFER_SIZE + 64];
    size_t total_len = len;
    if (len < sizeof(buffer) - 2 && (len == 0 || data[len-1] != '\n')) {
        memcpy(buffer, data, len);
        buffer[len] = '\n';
        buffer[len+1] = '\0';
        total_len = len + 1;
        data = buffer;
    }
    
    // Send with partial write handling
    size_t sent = 0;
    while (sent < total_len) {
        int result = send(ctx->socket_fd, data + sent, total_len - sent, 0);
        if (result > 0) {
            sent += result;
        } else if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            return -1;
        } else {
            break;
        }
    }
    return sent;
}

int net_receive(ClientContext* ctx, char* buffer, size_t len) {
    if (!ctx || ctx->socket_fd == -1) return -1;
    return recv(ctx->socket_fd, buffer, len, 0);
}

// Non-blocking receive with timeout (in milliseconds)
int net_receive_nonblocking(ClientContext* ctx, char* buffer, size_t len, int timeout_ms) {
    if (!ctx || ctx->socket_fd == -1) return -1;
    
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(ctx->socket_fd, &read_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(ctx->socket_fd + 1, &read_fds, NULL, NULL, &tv);
    if (result > 0 && FD_ISSET(ctx->socket_fd, &read_fds)) {
        int bytes = recv(ctx->socket_fd, buffer, len - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            return bytes;
        } else if (bytes == 0) {
            // Connection closed by server
            ctx->connected = false;
            return -2;
        }
    }
    return 0; // No data available or timeout
}

void net_close(ClientContext* ctx) {
    if (ctx && ctx->socket_fd != -1) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
        ctx->connected = false;
    }
}
