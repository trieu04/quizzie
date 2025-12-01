#ifndef STORAGE_H
#define STORAGE_H

#include "server.h"

// Function prototypes
int storage_load_questions(const char* filename, char* questions, char* answers);
int storage_save_log(const char* log_entry);

#endif // STORAGE_H