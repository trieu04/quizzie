#ifndef NET_H
#define NET_H

#include "client.h"

// Function prototypes
int net_connect(ClientContext* ctx, const char* ip, int port);
int net_send(ClientContext* ctx, const char* data, size_t len);
int net_receive(ClientContext* ctx, char* buffer, size_t len);
int net_receive_nonblocking(ClientContext* ctx, char* buffer, size_t len, int timeout_ms);
void net_close(ClientContext* ctx);

#endif // NET_H
