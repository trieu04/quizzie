#ifndef NET_H
#define NET_H

#include "server.h"

// Function prototypes
int net_setup(ServerContext* ctx);
int net_accept_client(ServerContext* ctx);
int net_send_to_client(int sock, const char* data, size_t len);
int net_receive_from_client(int sock, char* buffer, size_t len);
void net_close_client(ServerContext* ctx, int sock);

#endif // NET_H