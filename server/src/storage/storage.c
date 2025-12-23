#include "../../include/storage.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>

int storage_load_questions(const char* filename, char* questions, char* answers) {
    // If CSV extension, use CSV loader with randomization, limit to 20 questions
    size_t len = strlen(filename);
    if (len >= 4 && strcasecmp(filename + len - 4, ".csv") == 0) {
        return storage_load_questions_csv(filename, questions, answers, 20);
    }

    FILE* file = fopen(filename, "r");
    if (!file) {
        // Downgrade to INFO because callers often try multiple fallback paths
        LOG_INFO("storage_load_questions: fopen failed (will try fallbacks if any)");
        LOG_INFO(filename);
        LOG_INFO(strerror(errno));
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

        char id[32], q[512], optsA[256], optsB[256], optsC[256], optsD[256], ans[32];
        // Legacy TXT format: ID;Question;A.opt|B.opt|C.opt|D.opt;Answer
        char opts[1024];
        if (sscanf(line, "%[^;];%[^;];%[^;];%s", id, q, opts, ans) == 4) {
            if (question_count > 0) strcat(questions, ";");
            strcat(questions, q);
            strcat(questions, "?");
            strcat(questions, opts);
            if (strlen(answers) > 0) strcat(answers, ",");
            strcat(answers, ans);
            question_count++;
        }
        // Basic CSV: id,question,A,B,C,D,answer (no commas inside fields)
        else if (sscanf(line, "%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%s", id, q, optsA, optsB, optsC, optsD, ans) == 7) {
            if (question_count > 0) strcat(questions, ";");
            strcat(questions, q);
            strcat(questions, "?");
            // Build options with A.|B.|C.|D. labels, clamping parts to avoid truncation warnings
            char a[251], b[251], c[251], d[251];
            a[0] = b[0] = c[0] = d[0] = '\0';
            strncat(a, optsA, sizeof(a) - 1);
            strncat(b, optsB, sizeof(b) - 1);
            strncat(c, optsC, sizeof(c) - 1);
            strncat(d, optsD, sizeof(d) - 1);
            char opts_line[1024];
            snprintf(opts_line, sizeof(opts_line), "A.%s|B.%s|C.%s|D.%s", a, b, c, d);
            strcat(questions, opts_line);
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
    const char* paths[] = {"data/logs.txt", "../data/logs.txt", "../../data/logs.txt", NULL};
    FILE* file = NULL;
    for (int i = 0; paths[i] != NULL; i++) {
        file = fopen(paths[i], "a");
        if (file) break;
    }
    if (!file) return -1;
    fprintf(file, "%s\n", log_entry);
    fclose(file);
    return 0;
}

int storage_register_user(const char* username, const char* password) {
    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        return -1;
    }
    
    // Check if user already exists
    const char* paths[] = {"data/users.txt", "../data/users.txt", "../../data/users.txt", NULL};
    FILE* file = NULL;
    const char* user_path = NULL;
    
    for (int i = 0; paths[i] != NULL; i++) {
        file = fopen(paths[i], "r");
        if (file || (file = fopen(paths[i], "a+"))) {
            user_path = paths[i];
            break;
        }
    }
    if (!file) return -1;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char stored_user[64];
        if (sscanf(line, "%[^:]:", stored_user) == 1) {
            if (strcmp(stored_user, username) == 0) {
                fclose(file);
                return -2; // User already exists
            }
        }
    }
    fclose(file);
    
    // Add new user with participant role
    file = fopen(user_path, "a");
    if (!file) return -1;
    fprintf(file, "%s:%s:participant\n", username, password);
    fclose(file);
    
    LOG_INFO("User registered successfully");
    return 0;
}

int storage_verify_login(const char* username, const char* password, int* out_role) {
    if (!username || !password) return -1;
    
    // Check users from file with format: username:password:role
    const char* paths[] = {"data/users.txt", "../data/users.txt", "../../data/users.txt", NULL};
    FILE* file = NULL;
    
    for (int i = 0; paths[i] != NULL; i++) {
        file = fopen(paths[i], "r");
        if (file) break;
    }
    if (!file) return -1;
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\r\n")] = '\0';
        
        char stored_user[64], stored_pass[64], stored_role[32];
        // Parse username:password:role
        int parsed = sscanf(line, "%[^:]:%[^:]:%s", stored_user, stored_pass, stored_role);
        
        if (parsed >= 2 && strcmp(stored_user, username) == 0 && strcmp(stored_pass, password) == 0) {
            fclose(file);
            if (out_role) {
                // Check role if present, default to participant
                if (parsed == 3 && strcmp(stored_role, "admin") == 0) {
                    *out_role = 1; // ROLE_ADMIN
                } else {
                    *out_role = 0; // ROLE_PARTICIPANT
                }
            }
            return 0; // Login successful
        }
    }
    fclose(file);
    return -1; // Login failed
}

// --- CSV parsing and randomization helpers ---
static int parse_csv_fields(const char* line, char** out_fields, int max_fields) {
    int count = 0;
    const char* p = line;
    while (*p && count < max_fields) {
        // Skip leading spaces
        while (*p == ' ' || *p == '\t') p++;
        const char* start;
        char* field;
        size_t len;
        if (*p == '"') {
            p++; // skip opening quote
            start = p;
            // parse until closing quote, handle doubled quotes
            char buffer[1024];
            size_t bi = 0;
            while (*p) {
                if (*p == '"') {
                    if (*(p + 1) == '"') { // escaped quote
                        if (bi < sizeof(buffer) - 1) buffer[bi++] = '"';
                        p += 2;
                        continue;
                    } else {
                        p++; // consume closing quote
                        break;
                    }
                } else {
                    if (bi < sizeof(buffer) - 1) buffer[bi++] = *p;
                    p++;
                }
            }
            buffer[bi] = '\0';
            // allocate field
            len = strlen(buffer);
            field = (char*)malloc(len + 1);
            if (!field) return count;
            memcpy(field, buffer, len + 1);
            out_fields[count++] = field;
            // skip until comma
            while (*p && *p != ',') p++;
            if (*p == ',') p++;
        } else {
            start = p;
            while (*p && *p != ',') p++;
            len = (size_t)(p - start);
            // trim trailing spaces
            while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) len--;
            field = (char*)malloc(len + 1);
            if (!field) return count;
            memcpy(field, start, len);
            field[len] = '\0';
            out_fields[count++] = field;
            if (*p == ',') p++;
        }
    }
    return count;
}

static void free_fields(char** fields, int n) {
    for (int i = 0; i < n; i++) {
        if (fields[i]) free(fields[i]);
    }
}

static void shuffle_indices(int* idx, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
    }
}

int storage_load_questions_csv(const char* filename, char* questions, char* answers, int max_questions) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        LOG_ERROR("storage_load_questions_csv: fopen failed");
        LOG_ERROR(filename);
        LOG_ERROR(strerror(errno));
        return -1;
    }

    // Read all lines into memory (simple approach)
    typedef struct { char* q; char* A; char* B; char* C; char* D; char ans; } Row;
    Row* rows = NULL;
    int rows_cap = 0, rows_len = 0;
    char line[2048];
    // Optional: skip header if present (detect non-numeric ID in first column)
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n\r")] = 0;
        if (line[0] == '\0') continue;
        char* fields[8] = {0};
        int nf = parse_csv_fields(line, fields, 8);
        if (nf < 7) { free_fields(fields, nf); continue; }
        // fields: id, question, A, B, C, D, answer
        if (rows_len == rows_cap) {
            rows_cap = rows_cap ? rows_cap * 2 : 32;
            rows = (Row*)realloc(rows, rows_cap * sizeof(Row));
            if (!rows) { free_fields(fields, nf); fclose(file); return -1; }
        }
        Row r;
        r.q = fields[1];
        r.A = fields[2];
        r.B = fields[3];
        r.C = fields[4];
        r.D = fields[5];
        r.ans = fields[6][0]; // first char, e.g., 'A','B','C','D'
        // Free unused fields[0] (id) and any surplus
        free(fields[0]);
        for (int i = 7; i < nf; ++i) free(fields[i]);
        rows[rows_len++] = r;
    }
    fclose(file);

    if (rows_len == 0) {
        LOG_ERROR("CSV contained no questions");
        free(rows);
        return -1;
    }

    // Randomize selection
    srand((unsigned)time(NULL));
    int* idx = (int*)malloc(rows_len * sizeof(int));
    if (!idx) { free(rows); return -1; }
    for (int i = 0; i < rows_len; ++i) idx[i] = i;
    shuffle_indices(idx, rows_len);
    int take = rows_len < max_questions ? rows_len : max_questions;

    // Build outputs
    questions[0] = '\0';
    answers[0] = '\0';
    int count = 0;
    for (int k = 0; k < take; ++k) {
        Row* r = &rows[idx[k]];
        if (count > 0) strcat(questions, ";");
        // Question text and options
        strcat(questions, r->q);
        strcat(questions, "?");
        char a[251], b[251], c[251], d[251];
        a[0] = b[0] = c[0] = d[0] = '\0';
        strncat(a, r->A, sizeof(a) - 1);
        strncat(b, r->B, sizeof(b) - 1);
        strncat(c, r->C, sizeof(c) - 1);
        strncat(d, r->D, sizeof(d) - 1);
        char opts_line[1024];
        snprintf(opts_line, sizeof(opts_line), "A.%s|B.%s|C.%s|D.%s", a, b, c, d);
        strcat(questions, opts_line);
        if (answers[0] != '\0') strcat(answers, ",");
        char ansbuf[2] = { r->ans, '\0' };
        strcat(answers, ansbuf);
        count++;
        // Guard against buffer overflow
        if (strlen(questions) > BUFFER_SIZE - 64) break;
        if (strlen(answers) > 48) break;
    }

    // Free rows
    for (int i = 0; i < rows_len; ++i) {
        free(rows[i].q);
        free(rows[i].A);
        free(rows[i].B);
        free(rows[i].C);
        free(rows[i].D);
    }
    free(rows);
    free(idx);

    char info[64];
    sprintf(info, "Loaded %d randomized questions (CSV)", count);
    LOG_INFO(info);
    return count > 0 ? 0 : -1;
}

// Save uploaded CSV data to a question bank file
int storage_save_csv_bank(const char* filename, const char* csv_data) {
    if (!filename || !csv_data) return -1;
    
    // Try multiple paths for data directory
    const char* prefixes[] = {"data/", "../data/", "../../data/", NULL};
    char full_path[256];
    FILE* file = NULL;
    
    for (int i = 0; prefixes[i] != NULL; i++) {
        snprintf(full_path, sizeof(full_path), "%s%s", prefixes[i], filename);
        file = fopen(full_path, "w");
        if (file) break;
    }
    
    if (!file) {
        LOG_ERROR("Failed to open file for writing CSV bank");
        return -1;
    }
    
    // Write CSV data
    fprintf(file, "%s", csv_data);
    fclose(file);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Saved CSV bank: %s", filename);
    LOG_INFO(log_msg);
    storage_save_log(log_msg);
    
    return 0;
}