#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

// Common constants
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 32768  // 32KB

// String buffer sizes (match server for compatibility)
#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 64
#define MAX_FILENAME_LEN 128
#define MAX_SUBJECT_LEN 64

// Error handling macro
#define LOG_ERROR(msg) fprintf(stderr, "[ERROR] %s\n", msg)
#define LOG_INFO(msg) fprintf(stdout, "[INFO] %s\n", msg)

#endif // COMMON_H
