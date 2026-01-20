#ifndef STORAGE_H
#define STORAGE_H

#include "cJSON.h"

typedef struct
{
    char id[32];
    char name[64];
    long start_time;
    long end_time;
    char question_bank_id[32];
    char status[16]; // "WAITING" or "OPEN" or "CLOSED"
    int num_questions;
    int allowed_attempts;
    int duration; // in minutes
    int show_answers; // 1 or 0
} Room;

// User Management
void storage_init();
int storage_load_users(const char* filename);
int storage_check_credentials(const char* username, const char* password);
int storage_add_user(const char* username, const char* password, const char* role);
int storage_user_exists(const char* username);
const char* storage_get_role(const char* username);

// Room Management
int storage_save_room(const Room* room);
int storage_get_rooms(cJSON* rooms_array);
int storage_update_room_status(const char* room_id, const char* status);
int storage_delete_room(const char* room_id);
cJSON* storage_get_room(const char* room_id);

// Result Management
typedef struct
{
    char room_id[32];
    char username[32];
    int score;
    int num_questions;
    int correct_count;
    long timestamp;
    cJSON* questions; // Array of questions with correct answers
    cJSON* answers;   // Array of user's answers
} RoomResult;

int storage_save_result(const RoomResult* result);
int storage_get_room_results(const char* room_id, cJSON* results_array);
int storage_get_user_attempts(const char* room_id, const char* username);

// Question Management
int storage_save_question_bank(const char* bank_name, cJSON* questions);
int storage_list_question_banks(cJSON* banks_array);
int storage_get_question_bank(const char* bank_id, cJSON** questions);
int storage_update_question_bank(const char* bank_id, cJSON* questions);
int storage_delete_question_bank(const char* bank_id);

// Exam Session Management
typedef struct
{
    char room_id[32];
    char username[32];
    cJSON* questions; // Array of questions assigned to this user
    cJSON* answers;   // Array of user answers
    long start_time;
    int is_finished;
} ExamSession;

int storage_save_exam_session(const ExamSession* session);
ExamSession* storage_get_exam_session(const char* room_id, const char* username);
int storage_delete_exam_session(const char* room_id, const char* username);
void storage_free_exam_session(ExamSession* session);

#endif
