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
    int question_count = 0;

    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n\r")] = 0;

        if (strlen(line) == 0) continue;

        char id[10], q[500], opts[500], ans[10];
        // Format: ID;Question;A.opt|B.opt|C.opt|D.opt;Answer
        if (sscanf(line, "%[^;];%[^;];%[^;];%s", id, q, opts, ans) == 4) {
            // Build question string with options embedded
            // Format sent to client: "Question Text?A.opt1|B.opt2|C.opt3|D.opt4"
            if (question_count > 0) {
                strcat(questions, ";");
            }
            strcat(questions, q);
            strcat(questions, "?");
            strcat(questions, opts);

            if (strlen(answers) > 0) strcat(answers, ",");
            strcat(answers, ans);
            question_count++;
        }
    }
    fclose(file);

    if (question_count == 0) {
        LOG_ERROR("No questions loaded from file");
        return -1;
    }

    char info[64];
    sprintf(info, "Loaded %d questions", question_count);
    LOG_INFO(info);
    return 0;
}

int storage_save_log(const char* log_entry) {
    FILE* file = fopen("../../data/logs.txt", "a");
    if (!file) return -1;
    fprintf(file, "%s\n", log_entry);
    fclose(file);
    return 0;
}