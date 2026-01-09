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
    char status[16]; // "OPEN" or "CLOSED"
    int num_questions;
    int allowed_attempts;
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
    long timestamp;
} RoomResult;

int storage_save_result(const RoomResult* result);
int storage_get_room_results(const char* room_id, cJSON* results_array);

// Question Management
int storage_save_question_bank(const char* bank_name, cJSON* questions);
int storage_list_question_banks(cJSON* banks_array);
int storage_get_question_bank(const char* bank_id, cJSON** questions);
int storage_update_question_bank(const char* bank_id, cJSON* questions);
int storage_delete_question_bank(const char* bank_id);

#endif
