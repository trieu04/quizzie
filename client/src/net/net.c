#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

void net_connect(const char *host, int port) {
    (void)host;
    (void)port;
    printf("Connecting to %s:%d\n", host, port);
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
    uint32_t payload_len = total_len - HEADER_SIZE;

    if (payload_len > 128 * 1024) return -2;

    strncpy(msg_type_out, header.msg_type, 3);
    msg_type_out[3] = '\0';

    char *buffer = malloc(payload_len + 1);
    if (!buffer) return -1;

    bytes_read = recv(sock, buffer, payload_len, MSG_WAITALL);
    if (bytes_read != (ssize_t)payload_len) {
        free(buffer);
        return -1;
    }
    buffer[payload_len] = '\0';

    *payload_out = cJSON_Parse(buffer);
    free(buffer);

    return (*payload_out) ? 0 : -3;
}
