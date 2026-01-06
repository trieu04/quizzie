#ifndef SERVER_NET_H
#define SERVER_NET_H

#include "cJSON.h"
#include "protocol.h"

int net_listen(int port);
int send_packet(int sock, const char *msg_type, cJSON *payload);
int receive_packet(int sock, char *msg_type_out, cJSON **payload_out);

#endif
