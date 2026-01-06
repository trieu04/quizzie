#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

// Function prototypes
int storage_load_questions(const char* filename, char* questions, char* answers);
int storage_save_log(const char* log_entry);
void storage_log_with_timestamp(const char* fmt, ...);
int storage_save_result(int room_id, const char* host, const char* username, int score, int total, int time_taken);
// Load questions from a CSV file with randomization; limits to max_questions
int storage_load_questions_csv(const char* filename, char* questions, char* answers, int max_questions);
// Load practice questions filtered by difficulty counts
int storage_load_practice_questions(const char* filename, int easy_count, int med_count, int hard_count,
                                     char* questions, char* answers);
int storage_register_user(const char* username, const char* password);
int storage_verify_login(const char* username, const char* password, int* out_role);
// Load filtered questions for room creation (Easy/Med/Hard, Randomize)
int storage_load_filtered_questions(const char* filename, int easy, int med, int hard, 
                                    bool randomize_answers, char* questions, char* answers);
// Get list of available CSV question files
int storage_get_question_files(char* buffer, int max_len);
// Get list of practice subjects (from data/questions/practice/)
int storage_get_practice_subjects(char* buffer, int max_len);
int storage_save_csv_bank(const char* filename, const char* csv_data);
// Save complete server state (rooms, clients, progress)
int storage_save_server_state(const ServerContext* ctx);
// Load complete server state
int storage_load_server_state(ServerContext* ctx);

#endif // STORAGE_H