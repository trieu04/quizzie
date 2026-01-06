#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"
#include <netinet/in.h>
#include <time.h>

// Forward declarations
struct ClientContext;

typedef enum {
    PAGE_LOGIN,
    PAGE_REGISTER,     // Register page
    PAGE_DASHBOARD,
    PAGE_PRACTICE,
    PAGE_ROOM_LIST,
    PAGE_QUIZ,
    PAGE_RESULT,
    PAGE_HOST_PANEL,  // New: Host control panel
    PAGE_ADMIN_PANEL,  // Admin panel for room management
    PAGE_FILE_PREVIEW, // File preview page
    PAGE_HISTORY,       // Test history page
    PAGE_CREATE_ROOM   // New: Create Room configuration page
} AppState;

typedef enum {
    QUIZ_STATE_WAITING = 0,
    QUIZ_STATE_STARTED = 1,
    QUIZ_STATE_FINISHED = 2
} QuizState;

typedef struct {
    char name[128];
    int easy_cnt;
    int med_cnt;
    int hard_cnt;
    int total_cnt;
} QuestionFile;

#define MAX_QUESTIONS 100
#define MAX_ROOMS_DISPLAY 20
#define MAX_PARTICIPANTS 20

typedef struct {
    int id;
    char question[512];
    char options[4][256];
    char correct_answer;
    char difficulty[16];
} Question;

typedef struct {
    int id;
    char host_username[MAX_USERNAME_LEN];
    int player_count;
    int state;         // 0: waiting, 1: started, 2: finished
    bool is_my_room;   // True if current user is the host
} RoomInfo;

typedef struct {
    char username[MAX_USERNAME_LEN];
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
    bool force_page_refresh;     // Force page navigation even if state unchanged
    
    // Server connection
    char server_ip[MAX_FILENAME_LEN];          // Server IP or domain
    int server_port;             // Server port
    
    // User state
    char username[MAX_USERNAME_LEN];
    int role;                    // 0 = participant, 1 = admin
    
    // Room state
    int current_room_id;
    int last_room_id;            // For reconnect attempt
    bool is_host;
    QuizState room_state;
    RoomInfo rooms[MAX_ROOMS_DISPLAY];
    int room_count;
    
    // Host panel state
    int quiz_duration;           // Quiz duration in seconds
    char question_file[MAX_FILENAME_LEN];      // Selected question file
    char subject[MAX_SUBJECT_LEN];            // Selected subject (practice/exam)
    ParticipantInfo participants[MAX_PARTICIPANTS];
    int participant_count;
    int stats_waiting;
    int stats_taking;
    int stats_submitted;
    int stats_avg_percent;
    int stats_best_percent;
    int stats_last_percent;
    
    // Quiz state
    Question questions[MAX_QUESTIONS];
    int question_count;
    int current_question;
    char answers[MAX_QUESTIONS * 2];
    int score;
    int total_questions;
    time_t quiz_start_time;      // When client started their quiz
    int time_taken;              // Time taken to complete quiz
    bool is_practice;            // Practice mode flag
    char practice_answers[MAX_QUESTIONS * 2];  // Correct answers for practice mode
    
    // Quiz availability
    bool quiz_available;         // Whether host has started quiz
    
    // Message buffer for server responses
    char message_buffer[BUFFER_SIZE];
    bool has_pending_message;
    char recv_buffer[BUFFER_SIZE * 2];  // Buffer for incomplete messages
    int recv_len;                        // Current buffer length
    
    // Status message for UI
    char status_message[128];
    
    // Available files handling
    char available_files[4096];
    QuestionFile available_files_list[50];
    int available_files_count;
    bool files_refreshed;
    
    // Practice subjects handling
    char practice_subjects[1024];
    struct {
        char name[MAX_SUBJECT_LEN];
        int easy_count;
        int medium_count;
        int hard_count;
    } practice_subjects_list[50];
    int practice_subjects_count;
    bool subjects_refreshed;
} ClientContext;

// Function prototypes
ClientContext* client_init();
void client_cleanup(ClientContext* ctx);
void client_run(ClientContext* ctx);

// Network helper functions
int client_send_message(ClientContext* ctx, const char* type, const char* data);
int client_receive_message(ClientContext* ctx);
void client_process_server_message(ClientContext* ctx, const char* message);

// Storage functions
int save_quiz_result(ClientContext* ctx, const char* quiz_type, const char* subject);

#endif // CLIENT_H
