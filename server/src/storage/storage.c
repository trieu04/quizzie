#include "../../include/storage.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

int storage_load_questions(const char* filename, char* questions, char* answers) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("storage_load_questions: fopen failed");
        LOG_ERROR(filename);
        LOG_ERROR(strerror(errno));
        return -1;
    }

    strcpy(questions, "");
    strcpy(answers, "");
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char id[10], q[500], opts[500], ans[10];
        sscanf(line, "%[^;];%[^;];%[^;];%s", id, q, opts, ans);
        strcat(questions, q);
        strcat(questions, ";");
        if (strlen(answers) > 0) strcat(answers, ",");
        strcat(answers, ans);
    }
    fclose(file);
    return 0;
}

int storage_save_log(const char* log_entry) {
    FILE* file = fopen("../../data/logs.txt", "a");
    if (!file) return -1;
    fprintf(file, "%s\n", log_entry);
    fclose(file);
    return 0;
}