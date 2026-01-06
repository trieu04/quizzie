#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MSG_TYPE_REQ "REQ"
#define MSG_TYPE_RES "RES"
#define MSG_TYPE_UPD "UPD"

// Fixed header size = 4 bytes length + 3 bytes type = 7 bytes
#define HEADER_SIZE 7

typedef struct {
    uint32_t total_length; // Includes header size + payload size (Network Byte Order)
    char msg_type[3];
} __attribute__((packed)) PacketHeader;

#endif
