#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <netinet/in.h>

// Forward declarations
struct ClientContext;

typedef enum {
    PAGE_LOGIN,
    PAGE_DASHBOARD,
    PAGE_ROOM_LIST,
    PAGE_QUIZ,
    PAGE_RESULT
} AppState;

#define MAX_QUESTIONS 20
#define MAX_ROOMS_DISPLAY 10

typedef struct {
    int id;
    char question[256];
    char options[4][128];
    char correct_answer;
} Question;

typedef struct {
    int id;
    int player_count;
} RoomInfo;

typedef struct ClientContext {
    int socket_fd;
    struct sockaddr_in server_addr;
    bool running;
    bool connected;
    AppState current_state;
    
    // User state
    char username[32];
    
    // Room state
    int current_room_id;
    bool is_host;
    RoomInfo rooms[MAX_ROOMS_DISPLAY];
    int room_count;
    
    // Quiz state
    Question questions[MAX_QUESTIONS];
    int question_count;
    int current_question;
    char answers[MAX_QUESTIONS];
    int score;
    int total_questions;
    
    // Message buffer for server responses
    char message_buffer[BUFFER_SIZE];
    bool has_pending_message;
    
    // Status message for UI
    char status_message[128];
} ClientContext;

// Function prototypes
ClientContext* client_init();
void client_cleanup(ClientContext* ctx);
void client_run(ClientContext* ctx);

// Network helper functions
int client_send_message(ClientContext* ctx, const char* type, const char* data);
int client_receive_message(ClientContext* ctx);
void client_process_server_message(ClientContext* ctx, const char* message);

#endif // CLIENT_H
