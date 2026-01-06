#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

int net_listen(int port) {
    // Placeholder
    printf("Listening on port %d\n", port);
    return 0;
}

int send_packet(int sock, const char *msg_type, cJSON *payload) {
    char *json_str = cJSON_PrintUnformatted(payload);
    if (!json_str) return -1;

    uint32_t payload_len = strlen(json_str);
    uint32_t total_len = HEADER_SIZE + payload_len;

    PacketHeader header;
    header.total_length = htonl(total_len);
    memset(header.msg_type, 0, 3);
    memcpy(header.msg_type, msg_type, 3); // Copy exactly 3 bytes

    // Send Header
    if (send(sock, &header, HEADER_SIZE, 0) != HEADER_SIZE) {
        free(json_str);
        return -1;
    }

    // Send Payload
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

    // Check bounds (128KB limit as per SRS)
    if (payload_len > 128 * 1024) return -2;

    strncpy(msg_type_out, header.msg_type, 3);
    msg_type_out[3] = '\0'; // Ensure null-terminated for caller convenience

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
