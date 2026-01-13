#include "client_manager.h"
#include "net.h"
#include "server.h"
#include "storage.h"
#include "logger.h"
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define POLL_TIMEOUT_MS 5000 // 5 seconds to check room status periodically
#define DEFAULT_PORT 8080
#define ROOM_CHECK_INTERVAL 5 // Check rooms every 5 seconds

int main(int argc, char* argv[])
{
    signal(SIGPIPE, SIG_IGN);

    logger_init("server.log");

    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    storage_init();
    server_start(port);

    logger_cleanup();
    return 0;
}

void check_and_close_expired_rooms()
{
    cJSON* rooms = cJSON_CreateArray();
    storage_get_rooms(rooms);

    time_t now = time(NULL);
    cJSON* room;
    cJSON_ArrayForEach(room, rooms)
    {
        cJSON* id = cJSON_GetObjectItem(room, "id");
        cJSON* status = cJSON_GetObjectItem(room, "status");
        cJSON* start_time = cJSON_GetObjectItem(room, "start_time");
        cJSON* end_time = cJSON_GetObjectItem(room, "end_time");

        if (id && status && end_time) {
            // If room is WAITING and current time >= start_time, open it
            if (start_time && strcmp(status->valuestring, "WAITING") == 0 && now >= (time_t)start_time->valuedouble) {
                LOG_INFO("Auto-opening room %s (start at %ld, now is %ld)",
                       id->valuestring, (long)start_time->valuedouble, (long)now);
                storage_update_room_status(id->valuestring, "OPEN");
            }
            // If room is OPEN and current time > end_time, close it
            else if (strcmp(status->valuestring, "OPEN") == 0 && now > (time_t)end_time->valuedouble) {
                LOG_INFO("Auto-closing room %s (expired at %ld, now is %ld)",
                       id->valuestring, (long)end_time->valuedouble, (long)now);
                storage_update_room_status(id->valuestring, "CLOSED");
            }
        }
    }

    cJSON_Delete(rooms);
}

void server_start(int port)
{
    int server_fd = net_listen(port);
    if (server_fd < 0) {
        LOG_ERROR("Failed to start server");
        exit(1);
    }

    client_manager_init();

    // Setup poll server fd
    struct pollfd* fds = client_get_pollfds();
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    LOG_INFO("Server loop started on port %d...", port);

    time_t last_check = time(NULL);

    while (1) {
        int ret = poll(fds, MAX_CLIENTS + 1, POLL_TIMEOUT_MS);
        if (ret < 0) {
            LOG_ERROR("poll error: %s", strerror(errno));
            break;
        }

        // Periodically check and close expired rooms
        time_t now = time(NULL);
        if (now - last_check >= ROOM_CHECK_INTERVAL) {
            check_and_close_expired_rooms();
            last_check = now;
        }

        // Check for new connection
        if (fds[0].revents & POLLIN) {
            client_add(server_fd);
        }

        // Check clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            ClientState* client = client_get_state(i);
            if (client->fd != -1 && (fds[i + 1].revents & POLLIN)) {
                client_handle_activity(i);
            }
        }
    }

    close(server_fd);
}
