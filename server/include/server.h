#ifndef SERVER_H
#define SERVER_H

#include "common.h"
#include <sys/epoll.h>
#include <time.h>

// Quiz state enum
typedef enum {
    QUIZ_STATE_WAITING,    // Waiting for host to start
    QUIZ_STATE_STARTED,    // Quiz is active
    QUIZ_STATE_FINISHED    // Quiz ended
} QuizState;

// Structs
typedef struct {
    int sock;
    char username[50];
    time_t quiz_start_time;    // When this client started their quiz
    bool is_taking_quiz;       // Whether client is currently taking quiz
    bool has_submitted;        // Whether client has submitted answers
    int score;                 // Client's score
    int total;                 // Total questions
} Client;

typedef struct {
    int id;
    char host_username[50];    // Store host by username instead of socket
    int host_sock;             // Current host socket (can change on reconnect)
    Client *clients[MAX_CLIENTS];
    int client_count;
    char questions[BUFFER_SIZE];
    char correct_answers[50];
    int quiz_duration;         // Quiz duration in seconds (0 = unlimited)
    QuizState state;           // Current quiz state
    char question_file[128];   // Selected question file
} Room;

typedef struct {
    char type[20];
    char data[1000];
} Message;

typedef struct ServerContext {
    int server_fd;
    int epoll_fd;
    Client clients[MAX_CLIENTS];
    Room rooms[MAX_ROOMS];
    int client_count;
    int room_count;
    int next_room_id;  // Room ID counter
    bool running;
} ServerContext;

// Function prototypes
ServerContext* server_init();
void server_cleanup(ServerContext* ctx);
void server_run(ServerContext* ctx);

#endif // SERVER_H