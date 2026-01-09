#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEBUG_NET 1

int net_connect(const char *host, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        if (DEBUG_NET) printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        if (DEBUG_NET) printf("Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        if (DEBUG_NET) printf("Connection failed\n");
        return -1;
    }
    
    if (DEBUG_NET) printf("Connected to %s:%d\n", host, port);
    return sock;
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
    
    // calloc already ensures null terminator, but being explicit is fine
    buffer[payload_len] = '\0'; 

    *payload_out = cJSON_Parse(buffer);
    free(buffer);

    return (*payload_out) ? 0 : -3;
}
