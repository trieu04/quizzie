#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <netinet/in.h>
#include <time.h>

// Forward declarations
struct ClientContext;

typedef enum {
    PAGE_LOGIN,
    PAGE_DASHBOARD,
    PAGE_ROOM_LIST,
    PAGE_QUIZ,
    PAGE_RESULT,
    PAGE_HOST_PANEL  // New: Host control panel
} AppState;

typedef enum {
    QUIZ_STATE_WAITING = 0,
    QUIZ_STATE_STARTED = 1,
    QUIZ_STATE_FINISHED = 2
} QuizState;

#define MAX_QUESTIONS 20
#define MAX_ROOMS_DISPLAY 20
#define MAX_PARTICIPANTS 20

typedef struct {
    int id;
    char question[256];
    char options[4][128];
    char correct_answer;
} Question;

typedef struct {
    int id;
    char host_username[32];
    int player_count;
    int state;         // 0: waiting, 1: started, 2: finished
    bool is_my_room;   // True if current user is the host
} RoomInfo;

typedef struct {
    char username[50];
    char status;      // 'W' = waiting, 'T' = taking, 'S' = submitted
    int remaining_time;
    int score;
    int total;
} ParticipantInfo;

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
    QuizState room_state;
    RoomInfo rooms[MAX_ROOMS_DISPLAY];
    int room_count;
    
    // Host panel state
    int quiz_duration;           // Quiz duration in seconds
    char question_file[64];      // Selected question file
    ParticipantInfo participants[MAX_PARTICIPANTS];
    int participant_count;
    int stats_waiting;
    int stats_taking;
    int stats_submitted;
    
    // Quiz state
    Question questions[MAX_QUESTIONS];
    int question_count;
    int current_question;
    char answers[MAX_QUESTIONS];
    int score;
    int total_questions;
    time_t quiz_start_time;      // When client started their quiz
    int time_taken;              // Time taken to complete quiz
    
    // Quiz availability
    bool quiz_available;         // Whether host has started quiz
    
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
