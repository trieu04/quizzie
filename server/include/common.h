#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

// Server-specific constants
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define SERVER_BACKLOG 128  // Listen queue size
#define BUFFER_SIZE 32768  // 32KB - enough for large question sets
#define MAX_CLIENTS 100
#define MAX_ROOMS 10
#define MAX_QUESTIONS 100

// Default quiz settings
#define DEFAULT_QUIZ_DURATION 300  
#define DEFAULT_QUESTION_LIMIT 20

// String buffer sizes
#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 64
#define MAX_FILENAME_LEN 128

// Error handling macros
#define LOG_ERROR(msg) fprintf(stderr, "[ERROR] %s\n", msg)
#define LOG_INFO(msg) fprintf(stdout, "[INFO] %s\n", msg)

#endif // COMMON_H