#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include "cJSON.h"
#include "protocol.h"

int net_connect(const char *host, int port);
int send_packet(int sock, const char *msg_type, cJSON *payload);
int receive_packet(int sock, char *msg_type_out, cJSON **payload_out);

#endif
