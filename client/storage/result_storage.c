#include "../include/client.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

// Create results directory if it doesn't exist
static void ensure_results_dir() {
    const char* result_dirs[] = {
        "data/results",
        "../data/results",
        "../../data/results",
        NULL
    };
    
    for (int i = 0; result_dirs[i] != NULL; i++) {
        struct stat st = {0};
        if (stat(result_dirs[i], &st) == -1) {
            #ifdef _WIN32
            mkdir(result_dirs[i]);
            #else
            mkdir(result_dirs[i], 0755);
            #endif
        }
    }
}

// Save result to file
// Format: username_type_subject_timestamp.txt
// Returns: 0 on success, -1 on failure
int save_quiz_result(ClientContext* ctx, const char* quiz_type, const char* subject) {
    if (!ctx || !quiz_type || !subject) return -1;
    
    ensure_results_dir();
    
    // Generate filename
    time_t now = time(NULL);
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_%s_%s_%ld.txt", 
             ctx->username, quiz_type, subject, (long)now);
    
    // Try different result directories
    const char* result_dirs[] = {
        "data/results",
        "../data/results",
        "../../data/results",
        NULL
    };
    
    FILE* fp = NULL;
    char filepath[512];
    
    for (int i = 0; result_dirs[i] != NULL; i++) {
        snprintf(filepath, sizeof(filepath), "%s/%s", result_dirs[i], filename);
        fp = fopen(filepath, "w");
        if (fp) break;
    }
    
    if (!fp) return -1;
    
    // Write result information
    fprintf(fp, "Quiz Result\n");
    fprintf(fp, "====================\n");
    fprintf(fp, "User: %s\n", ctx->username);
    fprintf(fp, "Type: %s\n", quiz_type);
    fprintf(fp, "Subject: %s\n", subject);
    
    struct tm* timeinfo = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
    fprintf(fp, "Date: %s\n", time_str);
    
    fprintf(fp, "Score: %d / %d\n", ctx->score, ctx->total_questions);
    
    float percentage = ctx->total_questions > 0 ? 
                       (float)ctx->score / ctx->total_questions * 100.0f : 0.0f;
    fprintf(fp, "Percentage: %.1f%%\n", percentage);
    
    if (ctx->time_taken > 0) {
        int mins = ctx->time_taken / 60;
        int secs = ctx->time_taken % 60;
        fprintf(fp, "Time: %02d:%02d\n", mins, secs);
    }
    
    fprintf(fp, "====================\n");
    
    fclose(fp);
    return 0;
}
