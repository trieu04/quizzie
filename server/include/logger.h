#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_ERROR
} LogLevel;

void logger_init(const char* filename);
void logger_cleanup();
void logger_log(LogLevel level, const char* file, int line, const char* fmt, ...);

#define LOG_INFO(fmt, ...) logger_log(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
