// Client-side CSV loader with randomization for practice/exam
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <strings.h>
#include "../include/common.h"

#define CLIENT_DEFAULT_QUESTION_LIMIT 20

static int parse_csv_fields(const char* line, char** out_fields, int max_fields) {
    int count = 0;
    const char* p = line;
    while (*p && count < max_fields) {
        while (*p == ' ' || *p == '\t') p++;
        const char* start;
        char* field;
        size_t len;
        if (*p == '"') {
            p++;
            char buffer[1024];
            size_t bi = 0;
            while (*p) {
                if (*p == '"') {
                    if (*(p + 1) == '"') { buffer[bi++] = '"'; p += 2; }
                    else { p++; break; }
                } else { buffer[bi++] = *p++; }
                if (bi >= sizeof(buffer) - 1) break;
            }
            buffer[bi] = '\0';
            len = strlen(buffer);
            field = (char*)malloc(len + 1);
            if (!field) return count;
            memcpy(field, buffer, len + 1);
            out_fields[count++] = field;
            while (*p && *p != ',') p++;
            if (*p == ',') p++;
        } else {
            start = p;
            while (*p && *p != ',') p++;
            len = (size_t)(p - start);
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
    for (int i = 0; i < n; i++) { if (fields[i]) free(fields[i]); }
}

static void shuffle_indices(int* idx, int n) {
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
    }
}

int storage_load_questions(const char* filename, char* questions, char* answers) {
    // If CSV: id,question,A,B,C,D,answer
    size_t len = strlen(filename);
    if (len >= 4 && strcasecmp(filename + len - 4, ".csv") == 0) {
        FILE* file = fopen(filename, "r");
        if (!file) return -1;
        typedef struct { char* q; char* A; char* B; char* C; char* D; char ans; } Row;
        Row* rows = NULL; int rows_cap = 0, rows_len = 0;
        char line[2048];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n\r")] = 0; if (!line[0]) continue;
            char* fields[8] = {0}; int nf = parse_csv_fields(line, fields, 8);
            if (nf < 7) { free_fields(fields, nf); continue; }
            if (rows_len == rows_cap) { rows_cap = rows_cap ? rows_cap * 2 : 32; rows = (Row*)realloc(rows, rows_cap * sizeof(Row)); if (!rows) { free_fields(fields, nf); fclose(file); return -1; } }
            Row r; r.q = fields[1]; r.A = fields[2]; r.B = fields[3]; r.C = fields[4]; r.D = fields[5]; r.ans = fields[6][0];
            free(fields[0]); for (int i = 7; i < nf; ++i) free(fields[i]);
            rows[rows_len++] = r;
        }
        fclose(file);
        if (rows_len == 0) { free(rows); return -1; }
        srand((unsigned)time(NULL)); int* idx = (int*)malloc(rows_len * sizeof(int)); if (!idx) { free(rows); return -1; }
        for (int i = 0; i < rows_len; ++i) {
            idx[i] = i;
        }
        shuffle_indices(idx, rows_len);
        int take = rows_len < CLIENT_DEFAULT_QUESTION_LIMIT ? rows_len : CLIENT_DEFAULT_QUESTION_LIMIT;
        questions[0] = '\0'; answers[0] = '\0';
        int count = 0;
        size_t q_len = 0, a_len = 0;
        for (int k = 0; k < take; ++k) {
            Row* r = &rows[idx[k]];
            
            // Check buffer space before adding
            char temp_q[1200];
            char opts_line[1024];
            snprintf(opts_line, sizeof(opts_line), "A.%s|B.%s|C.%s|D.%s", r->A, r->B, r->C, r->D);
            int written = snprintf(temp_q, sizeof(temp_q), "%s%s?%s",
                                   count > 0 ? ";" : "", r->q, opts_line);
            
            if (written > 0 && q_len + (size_t)written < BUFFER_SIZE - 64) {
                strcat(questions, temp_q);
                q_len += written;
            } else {
                break;
            }
            
            // Add answer with bounds check
            if (a_len + 2 < 200) {
                if (count > 0) {
                    strcat(answers, ",");
                    a_len++;
                }
                char ab[2] = { r->ans, '\0' };
                strcat(answers, ab);
                a_len++;
            } else {
                break;
            }
            count++;
        }
        for (int i = 0; i < rows_len; ++i) { free(rows[i].q); free(rows[i].A); free(rows[i].B); free(rows[i].C); free(rows[i].D); }
        free(rows); free(idx);
        return count > 0 ? 0 : -1;
    }

    // Legacy TXT format fallback
    FILE* file = fopen(filename, "r");
    if (!file) return -1;
    questions[0] = '\0';
    answers[0] = '\0';
    char line[BUFFER_SIZE];
    int question_count = 0;
    size_t q_len = 0, a_len = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n\r")] = 0;
        if (!line[0]) continue;
        char id[32], q[512], opts[1024], ans[32];
        if (sscanf(line, "%[^;];%[^;];%[^;];%s", id, q, opts, ans) == 4) {
            // Calculate space needed
            size_t needed = strlen(q) + strlen(opts) + 2; // +2 for separator and ?
            if (question_count > 0) needed++; // +1 for semicolon
            
            if (q_len + needed < BUFFER_SIZE - 64 && a_len + strlen(ans) + 2 < 200) {
                if (question_count > 0) {
                    strcat(questions, ";");
                    q_len++;
                }
                strcat(questions, q);
                strcat(questions, "?");
                strcat(questions, opts);
                q_len += strlen(q) + strlen(opts) + 1;
                
                if (question_count > 0) {
                    strcat(answers, ",");
                    a_len++;
                }
                strcat(answers, ans);
                a_len += strlen(ans);
                question_count++;
            } else {
                break; // Buffer full
            }
        }
    }
    fclose(file);
    return question_count > 0 ? 0 : -1;
}
