#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

// Function prototypes
int storage_load_questions(const char* filename, char* questions, char* answers);
int storage_save_log(const char* log_entry);
// Load questions from a CSV file with randomization; limits to max_questions
int storage_load_questions_csv(const char* filename, char* questions, char* answers, int max_questions);
int storage_register_user(const char* username, const char* password);
int storage_verify_login(const char* username, const char* password, int* out_role);
int storage_save_csv_bank(const char* filename, const char* csv_data);

#endif // STORAGE_H