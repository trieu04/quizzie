#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

int net_listen(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        return -1;
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    // Listen
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int send_packet(int sock, const char *msg_type, cJSON *payload) {
    char *json_str = cJSON_PrintUnformatted(payload);
    if (!json_str) return -1;

    uint32_t payload_len = strlen(json_str);
    uint32_t total_len = HEADER_SIZE + payload_len;

    PacketHeader header;
    header.total_length = htonl(total_len);
    memset(header.msg_type, 0, 3);
    memcpy(header.msg_type, msg_type, 3);

    if (send(sock, &header, HEADER_SIZE, 0) != HEADER_SIZE) {
        free(json_str);
        return -1;
    }

    if (send(sock, json_str, payload_len, 0) != (ssize_t)payload_len) {
        free(json_str);
        return -1;
    }

    free(json_str);
    return 0;
}

int receive_packet(int sock, char *msg_type_out, cJSON **payload_out) {
    PacketHeader header;
    ssize_t bytes_read = recv(sock, &header, HEADER_SIZE, MSG_WAITALL);
    if (bytes_read != HEADER_SIZE) return -1;

    uint32_t total_len = ntohl(header.total_length);
    if (total_len < HEADER_SIZE) return -2;

    uint32_t payload_len = total_len - HEADER_SIZE;
    if (payload_len > MAX_PAYLOAD_SIZE) return -2;

    strncpy(msg_type_out, header.msg_type, 3);
    msg_type_out[3] = '\0';

    char *buffer = calloc(1, payload_len + 1);
    if (!buffer) return -1;

    bytes_read = recv(sock, buffer, payload_len, MSG_WAITALL);
    if (bytes_read != (ssize_t)payload_len) {
        free(buffer);
        return -1;
    }
    
    // calloc already ensures null terminator
    buffer[payload_len] = '\0';

    *payload_out = cJSON_Parse(buffer);
    free(buffer);

    return (*payload_out) ? 0 : -3; 
}
