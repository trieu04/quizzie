#include "logger.h"
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void logger_init(const char* filename) {
    log_file = fopen(filename, "a");
    if (!log_file) {
        perror("Could not open log file");
    }
}

void logger_cleanup() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void logger_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    pthread_mutex_lock(&log_mutex);

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    const char* level_str = (level == LOG_LEVEL_ERROR) ? "ERROR" : "INFO";
    FILE* out = (level == LOG_LEVEL_ERROR) ? stderr : stdout;

    // Format the message
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // Print to console
    fprintf(out, "[%s] [%s] %s\n", timestamp, level_str, message);
    fflush(out);

    // Print to file
    if (log_file) {
        fprintf(log_file, "[%s] [%s] [%s:%d] %s\n", timestamp, level_str, file, line, message);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}
