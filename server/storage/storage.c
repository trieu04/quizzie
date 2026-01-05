#include "../include/server.h"
#include "../include/storage.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/types.h>
#include <ctype.h>

static const char* LOG_PATHS[] = {
    "data/logs/logs.txt",
    "../data/logs/logs.txt",
    "../../data/logs/logs.txt",
    "data/logs.txt",
    "../data/logs.txt",
    NULL
};

static const char* RESULT_PATHS[] = {
    "data/results/results.csv",
    "../data/results/results.csv",
    "../../data/results/results.csv",
    "data/results.csv",
    "../data/results.csv",
    NULL
};

static const char* USER_PATHS[] = {
    "data/users/users.txt",
    "../data/users/users.txt",
    "../../data/users/users.txt",
    "data/users.txt",
    "../data/users.txt",
    NULL
};

static const char* QUESTION_WRITE_PREFIXES[] = {
    "data/questions/",
    "../data/questions/",
    "../../data/questions/",
    NULL
};

static FILE* fopen_first(const char** paths, const char* mode) {
    FILE* file = NULL;
    for (int i = 0; paths[i] != NULL; i++) {
        file = fopen(paths[i], mode);
        if (file) return file;
    }
    return NULL;
}

static void log_with_timestamp(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    char line[600];
    snprintf(line, sizeof(line), "[%s] %s", ts, buffer);
    storage_save_log(line);
}

// Internal structure for questions (fixed buffers for simplicity in filtering)
typedef struct { 
    char q[256]; 
    char opts[4][128]; 
    char ans; 
    char diff[16]; 
} StorageQRow;

static void pick_questions_by_diff(StorageQRow* all_rows, int all_cnt, int* picked_indices, 
                                   StorageQRow** selected, int* sel_cnt, 
                                   const char* diff_target, int n) {
    if (n <= 0) return;
    
    int candidates[200];
    int cand_cnt = 0;
    
    for(int i = 0; i < all_cnt; i++) {
        if (!picked_indices[i]) {
            bool match = false;
            // logic: input "easy", "medium", "hard", or "any"
            if (strcasecmp(diff_target, "any") == 0) {
                match = true;
            } else if (strcasecmp(diff_target, "medium") == 0) {
                // Matches "medium" OR anything not explicitly "easy" or "hard" (default fallback)
                // But strictly speaking, if we have explicit diffs, we match them.
                if (strncasecmp(all_rows[i].diff, "easy", 4) != 0 && 
                    strncasecmp(all_rows[i].diff, "hard", 4) != 0) {
                    match = true;
                }
            } else {
                // specific match for easy/hard
                if (strncasecmp(all_rows[i].diff, diff_target, strlen(diff_target)) == 0) {
                    match = true;
                }
            }
            
            if (match) {
                candidates[cand_cnt++] = i;
            }
        }
    }
    
    // Shuffle candidates
    for (int i = cand_cnt - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int t = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = t;
    }
    
    // Pick top n
    for (int i = 0; i < n && i < cand_cnt && *sel_cnt < MAX_QUESTIONS; i++) {
        int idx = candidates[i];
        picked_indices[idx] = 1;
        selected[(*sel_cnt)++] = &all_rows[idx];
    }
}

int storage_load_questions(const char* filename, char* questions, char* answers) {
    // If CSV extension, use CSV loader with randomization, limit to 20 questions
    size_t len = strlen(filename);
    if (len >= 4 && strcasecmp(filename + len - 4, ".csv") == 0) {
        return storage_load_questions_csv(filename, questions, answers, 20);
    }

    // Try multiple candidate paths for TXT format
    const char* prefixes[] = {
        "",
        "data/questions/",
        "../data/questions/",
        "../../data/questions/",
        "data/",
        "../data/",
        "../../data/",
        NULL
    };
    
    FILE* file = NULL;
    char filepath[512];
    for (int i = 0; prefixes[i] != NULL; ++i) {
        snprintf(filepath, sizeof(filepath), "%s%s", prefixes[i], filename);
        file = fopen(filepath, "r");
        if (file) break;
    }
    
    if (!file) {
        // Downgrade to INFO because callers often try multiple fallback paths
        LOG_INFO("storage_load_questions: fopen failed for all paths");
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
    FILE* file = fopen_first(LOG_PATHS, "a");
    if (!file) return -1;
    fprintf(file, "%s\n", log_entry);
    fclose(file);
    return 0;
}

int storage_save_result(int room_id, const char* host, const char* username, int score, int total, int time_taken) {
    FILE* file = fopen_first(RESULT_PATHS, "a");
    if (!file) return -1;

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(file, "%s,%d,%s,%s,%d,%d,%d\n", ts, room_id, host ? host : "", username ? username : "", score, total, time_taken);
    fclose(file);
    return 0;
}

int storage_register_user(const char* username, const char* password) {
    if (!username || !password || strlen(username) == 0 || strlen(password) == 0) {
        return -1;
    }
    
    FILE* file = NULL;
    const char* user_path = NULL;
    // Try to open an existing users file, or create in the first preferred path
    for (int i = 0; USER_PATHS[i] != NULL; i++) {
        file = fopen(USER_PATHS[i], "r");
        if (file) {
            user_path = USER_PATHS[i];
            break;
        }
    }
    if (!file) {
        // No existing file found; create in top-priority path
        user_path = USER_PATHS[0];
        file = fopen(user_path, "a+");
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
    log_with_timestamp("REGISTER user=%s", username);
    return 0;
}

int storage_verify_login(const char* username, const char* password, int* out_role) {
    if (!username || !password) return -1;
    
    // Check users from file with format: username:password:role
    FILE* file = fopen_first(USER_PATHS, "r");
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
            log_with_timestamp("LOGIN_SUCCESS user=%s role=%s", username, parsed == 3 ? stored_role : "participant");
            return 0; // Login successful
        }
    }
    fclose(file);
    log_with_timestamp("LOGIN_FAILED user=%s", username);
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
    // Try multiple candidate paths, including data/questions/ subdirectory structure
    const char* prefixes[] = {
        "",
        "data/questions/",
        "../data/questions/",
        "../../data/questions/",
        "data/",
        "../data/",
        "../../data/",
        NULL
    };
    
    FILE* file = NULL;
    char filepath[512];
    for (int i = 0; prefixes[i] != NULL; ++i) {
        snprintf(filepath, sizeof(filepath), "%s%s", prefixes[i], filename);
        file = fopen(filepath, "r");
        if (file) break;
    }
    
    if (!file) {
        LOG_ERROR("storage_load_questions_csv: fopen failed for all paths");
        LOG_ERROR(filename);
        LOG_ERROR(strerror(errno));
        return -1;
    }

    // Read all lines into memory (simple approach)
    typedef struct { char* q; char* A; char* B; char* C; char* D; char ans; char* diff; } Row;
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
        // fields: id, question, A, B, C, D, answer[,difficulty]
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
        r.diff = (nf >= 8 && fields[7]) ? fields[7] : NULL;
        // Free unused fields[0] (id) and any surplus
        free(fields[0]);
        for (int i = 8; i < nf; ++i) free(fields[i]);
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
        const char* diff = (r->diff && strlen(r->diff) > 0) ? r->diff : "medium";
        char opts_line[1024];
        snprintf(opts_line, sizeof(opts_line), "A.%s|B.%s|C.%s|D.%s^%s", a, b, c, d, diff);
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
        if (rows[i].diff) free(rows[i].diff);
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
    
    char full_path[256];
    FILE* file = NULL;
    
    for (int i = 0; QUESTION_WRITE_PREFIXES[i] != NULL; i++) {
        snprintf(full_path, sizeof(full_path), "%s%s", QUESTION_WRITE_PREFIXES[i], filename);
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

// Load practice questions filtered by difficulty
int storage_load_practice_questions(const char* filename, int easy_count, int med_count, int hard_count,
                                     char* questions, char* answers) {
    // Try multiple candidate paths
    const char* prefixes[] = {
        "",
        "data/questions/",
        "../data/questions/",
        "../../data/questions/",
        NULL
    };
    
    FILE* file = NULL;
    char filepath[512];
    for (int i = 0; prefixes[i] != NULL; ++i) {
        snprintf(filepath, sizeof(filepath), "%s%s", prefixes[i], filename);
        file = fopen(filepath, "r");
        if (file) break;
    }
    
    if (!file) {
        LOG_ERROR("storage_load_practice_questions: failed to open file");
        return -1;
    }

    // Parse CSV and bucket by difficulty
    // typedef struct { char q[256]; char opts[4][128]; char ans; char diff[16]; } QRow; // Use StorageQRow
    StorageQRow easy_rows[50], med_rows[50], hard_rows[50];
    int e_cnt = 0, m_cnt = 0, h_cnt = 0;
    
    char line[2048];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n\r")] = 0;
        if (line[0] == '\0' || line[0] == 'i') continue; // skip empty or header
        
        char* fields[8] = {0};
        int nf = parse_csv_fields(line, fields, 8);
        if (nf < 7) { free_fields(fields, nf); continue; }
        
        // Parse row: id, question, A, B, C, D, answer, difficulty
        StorageQRow qr;
        strncpy(qr.q, fields[1], sizeof(qr.q) - 1);
        qr.q[sizeof(qr.q) - 1] = '\0';
        for (int i = 0; i < 4; i++) {
            strncpy(qr.opts[i], fields[2 + i], sizeof(qr.opts[i]) - 1);
            qr.opts[i][sizeof(qr.opts[i]) - 1] = '\0';
        }
        qr.ans = fields[6][0];
        
        // Get difficulty (default medium)
        const char* diff_str = (nf >= 8 && fields[7]) ? fields[7] : "medium";
        strncpy(qr.diff, diff_str, sizeof(qr.diff) - 1);
        qr.diff[sizeof(qr.diff) - 1] = '\0';
        
        // Bucket by difficulty
        if (strncmp(qr.diff, "easy", 4) == 0 && e_cnt < 50) {
            easy_rows[e_cnt++] = qr;
        } else if (strncmp(qr.diff, "hard", 4) == 0 && h_cnt < 50) {
            hard_rows[h_cnt++] = qr;
        } else if (m_cnt < 50) {
            med_rows[m_cnt++] = qr;
        }
        
        free_fields(fields, nf);
    }
    fclose(file);
    
    // Shuffle each bucket
    int e_idx[50], m_idx[50], h_idx[50];
    for (int i = 0; i < e_cnt; i++) e_idx[i] = i;
    for (int i = 0; i < m_cnt; i++) m_idx[i] = i;
    for (int i = 0; i < h_cnt; i++) h_idx[i] = i;
    
    srand((unsigned)time(NULL));
    shuffle_indices(e_idx, e_cnt);
    shuffle_indices(m_idx, m_cnt);
    shuffle_indices(h_idx, h_cnt);
    
    // Build output string
    questions[0] = '\0';
    answers[0] = '\0';
    int out_count = 0;
    
    // Add easy questions
    int take_easy = (easy_count < e_cnt) ? easy_count : e_cnt;
    for (int i = 0; i < take_easy && out_count < MAX_QUESTIONS; i++) {
        StorageQRow* qr = &easy_rows[e_idx[i]];
        if (out_count > 0) strcat(questions, ";");
        strcat(questions, qr->q);
        strcat(questions, "?");
        char opts[512];
        snprintf(opts, sizeof(opts), "A.%s|B.%s|C.%s|D.%s^%s",
                 qr->opts[0], qr->opts[1], qr->opts[2], qr->opts[3], qr->diff);
        strcat(questions, opts);
        
        if (out_count > 0) strcat(answers, ",");
        char ans_str[2] = {qr->ans, '\0'};
        strcat(answers, ans_str);
        out_count++;
    }
    
    // Add medium questions
    int take_med = (med_count < m_cnt) ? med_count : m_cnt;
    for (int i = 0; i < take_med && out_count < MAX_QUESTIONS; i++) {
        StorageQRow* qr = &med_rows[m_idx[i]];
        if (out_count > 0) strcat(questions, ";");
        strcat(questions, qr->q);
        strcat(questions, "?");
        char opts[512];
        snprintf(opts, sizeof(opts), "A.%s|B.%s|C.%s|D.%s^%s",
                 qr->opts[0], qr->opts[1], qr->opts[2], qr->opts[3], qr->diff);
        strcat(questions, opts);
        
        if (out_count > 0) strcat(answers, ",");
        char ans_str[2] = {qr->ans, '\0'};
        strcat(answers, ans_str);
        out_count++;
    }
    
    // Add hard questions
    int take_hard = (hard_count < h_cnt) ? hard_count : h_cnt;
    for (int i = 0; i < take_hard && out_count < MAX_QUESTIONS; i++) {
        StorageQRow* qr = &hard_rows[h_idx[i]];
        if (out_count > 0) strcat(questions, ";");
        strcat(questions, qr->q);
        strcat(questions, "?");
        char opts[512];
        snprintf(opts, sizeof(opts), "A.%s|B.%s|C.%s|D.%s^%s",
                 qr->opts[0], qr->opts[1], qr->opts[2], qr->opts[3], qr->diff);
        strcat(questions, opts);
        
        if (out_count > 0) strcat(answers, ",");
        char ans_str[2] = {qr->ans, '\0'};
        strcat(answers, ans_str);
        out_count++;
    }
    
    return out_count > 0 ? 0 : -1;
}

int storage_get_question_files(char* buffer, int max_len) {
    // List .csv files in data/questions/ and count stats
    const char* path = "data/questions/";
    DIR* dir = opendir(path);
    if (!dir) {
        path = "../data/questions/";
        dir = opendir(path);
    }
    if (!dir) {
        path = "../../data/questions/";
        dir = opendir(path);
    }
    
    if (!dir) {
        buffer[0] = '\0';
        return -1;
    }
    
    buffer[0] = '\0';
    struct dirent* ent;
    int first = 1;
    
    while ((ent = readdir(dir)) != NULL) {
        // Check extension .csv
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcasecmp(ent->d_name + len - 4, ".csv") == 0) {
            // Open file to count stats
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s%s", path, ent->d_name);
            FILE* f = fopen(fullpath, "r");
            int e=0, m=0, h=0, t=0;
            if (f) {
                char line[2048];
                while(fgets(line, sizeof(line), f)) {
                    line[strcspn(line, "\n\r")] = 0;
                    if (line[0] == '\0' || line[0] == 'i') continue;
                    
                    // Simple search for difficulty without full parsing (faster)
                    // Format: ...,answer,difficulty
                    // Find last comma
                    char* last_comma = strrchr(line, ',');
                    if (last_comma) {
                         // Check difficulty
                         if (tolower(*(last_comma+1)) == 'e') e++;
                         else if (tolower(*(last_comma+1)) == 'h') h++;
                         else m++; // default medium
                         t++;
                    }
                }
                fclose(f);
            }
        
            // Check buffer space
            // Format: name|E,M,H,T
            char entry[256];
            snprintf(entry, sizeof(entry), "%s|%d,%d,%d,%d", ent->d_name, e, m, h, t);
            
            if (strlen(buffer) + strlen(entry) + 2 >= (size_t)max_len) break;
            
            if (!first) strcat(buffer, ";");
            strcat(buffer, entry);
            first = 0;
        }
    }
    
    closedir(dir);
    return 0;
}

int storage_load_filtered_questions(const char* filename, int easy_cnt, int med_cnt, int hard_cnt, int any_cnt, 
                                    bool randomize_answers, char* questions, char* answers) {
    // Similar to load_practice but handles "any" count and randomization of answers
    const char* prefixes[] = {
        "",
        "data/questions/",
        "../data/questions/",
        "../../data/questions/",
        "data/",
        "../data/",
        NULL
    };
    
    FILE* file = NULL;
    char filepath[512];
    for (int i = 0; prefixes[i] != NULL; ++i) {
        snprintf(filepath, sizeof(filepath), "%s%s", prefixes[i], filename);
        file = fopen(filepath, "r");
        if (file) break;
    }
    
    if (!file) {
        LOG_ERROR("storage_load_filtered_questions: failed to open file");
        return -1;
    }

    // Parse CSV and bucket by difficulty
    StorageQRow all_rows[200];
    int all_cnt = 0;
    
    char line[2048];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n\r")] = 0;
        if (line[0] == '\0' || line[0] == 'i') continue; // skip empty or header
        
        char* fields[8] = {0};
        int nf = parse_csv_fields(line, fields, 8);

// --- Server State Persistence ---

        if (nf < 7) { free_fields(fields, nf); continue; }
        
        StorageQRow qr;
        strncpy(qr.q, fields[1], sizeof(qr.q) - 1);
        qr.q[sizeof(qr.q) - 1] = '\0';
        for (int i = 0; i < 4; i++) {
            strncpy(qr.opts[i], fields[2 + i], sizeof(qr.opts[i]) - 1);
            qr.opts[i][sizeof(qr.opts[i]) - 1] = '\0';
        }
        qr.ans = fields[6][0];
        
        const char* diff_str = (nf >= 8 && fields[7]) ? fields[7] : "medium";
        strncpy(qr.diff, diff_str, sizeof(qr.diff) - 1);
        qr.diff[sizeof(qr.diff) - 1] = '\0';
        
        if (all_cnt < 200) all_rows[all_cnt++] = qr;
        
        free_fields(fields, nf);
    }
    fclose(file);
    
    // Helper to add questions
    int out_count = 0;
    questions[0] = '\0';
    answers[0] = '\0';
    
    int picked_indices[200] = {0}; // 1 if all_rows[i] is picked
    
    StorageQRow* selected[MAX_QUESTIONS];
    int sel_cnt = 0;
    
    srand((unsigned)time(NULL));
    
    if (easy_cnt > 0) pick_questions_by_diff(all_rows, all_cnt, picked_indices, selected, &sel_cnt, "easy", easy_cnt);
    if (med_cnt > 0) pick_questions_by_diff(all_rows, all_cnt, picked_indices, selected, &sel_cnt, "medium", med_cnt);
    if (hard_cnt > 0) pick_questions_by_diff(all_rows, all_cnt, picked_indices, selected, &sel_cnt, "hard", hard_cnt);
    if (any_cnt > 0) pick_questions_by_diff(all_rows, all_cnt, picked_indices, selected, &sel_cnt, "any", any_cnt);
    
    // Now write selected to output buffers
    for (int k = 0; k < sel_cnt; k++) {
        StorageQRow* r = selected[k];
        if (out_count > 0) strcat(questions, ";");
        // Question text and options
        strcat(questions, r->q);
        strcat(questions, "?");
        
        // Randomize options for this question if requested
        char* opt_ptrs[4] = { r->opts[0], r->opts[1], r->opts[2], r->opts[3] };
        char correct_char = r->ans; // 'A', 'B', 'C', or 'D'
        int correct_idx = correct_char - 'A';
        if (correct_idx < 0 || correct_idx > 3) correct_idx = 0; // fallback
        
        // If randomize options is TRUE
        // But wait, the task required "randomize answer order"
        // This usually means shuffling the options A,B,C,D.
        // We need to track where the correct answer went.
        
        int p[4] = {0, 1, 2, 3};
        if (randomize_answers) {
            for (int i = 3; i > 0; --i) {
                int j = rand() % (i + 1);
                int t = p[i];
                p[i] = p[j];
                p[j] = t;
            }
        }
        
        // Reconstruct A,B,C,D based on p
        char final_opts[512];
        snprintf(final_opts, sizeof(final_opts), "A.%s|B.%s|C.%s|D.%s^%s", 
                 opt_ptrs[p[0]], opt_ptrs[p[1]], opt_ptrs[p[2]], opt_ptrs[p[3]], r->diff);
        strcat(questions, final_opts);
        
        // New correct answer char
        int new_correct_idx = 0;
        for(int i=0; i<4; i++) {
            if (p[i] == correct_idx) {
                new_correct_idx = i;
                break;
            }
        }
        char new_ans_char = 'A' + new_correct_idx;
        
        if (answers[0] != '\0') strcat(answers, ",");
        char ansbuf[2] = { new_ans_char, '\0' };
        strcat(answers, ansbuf);
        out_count++;
    }
    
    return out_count > 0 ? 0 : -1;
}

// --- Server State Persistence ---

int storage_save_server_state(const ServerContext* ctx) {
    if (!ctx) return -1;

    FILE* file = fopen("data/server_state.save", "w");
    if (!file) {
        LOG_ERROR("Failed to open server_state.save for writing");
        return -1;
    }

    // Header: NEXT_ROOM_ID
    fprintf(file, "NEXT_ROOM_ID:%d\n", ctx->next_room_id);

    // Save Rooms
    for (int i = 0; i < ctx->room_count; i++) {
        const Room* r = &ctx->rooms[i];
        
        // Escape newlines in questions (simple approach: replace \n with [NL])
        // We need a buffer copy for this
        char* escaped_questions = (char*)malloc(strlen(r->questions) * 2 + 1); // rough estimate
        if (escaped_questions) {
            const char* src = r->questions;
            char* dst = escaped_questions;
            while (*src) {
                if (*src == '\n') {
                    strcpy(dst, "[NL]");
                    dst += 4;
                } else {
                    *dst++ = *src;
                }
                src++;
            }
            *dst = '\0';
        } else {
             escaped_questions = strdup(""); 
        }

        fprintf(file, "ROOM:%d,%s,%d,%d,%ld,%ld,%d,%d,%s,%s\n",
                r->id,
                r->host_username,
                r->quiz_duration,
                r->state,
                (long)r->start_time,
                (long)r->end_time,
                r->randomize_answers ? 1 : 0,
                r->client_count,
                r->question_file,
                r->correct_answers); 

        // Write questions on a separate line prefixed
        fprintf(file, "ROOM_QUESTIONS:%d:%s\n", r->id, escaped_questions);
        free(escaped_questions);

        // Save Participants in this room
        for (int j = 0; j < r->client_count; j++) {
            const Client* c = r->clients[j];
            if (c) {
                fprintf(file, "PARTICIPANT:%d,%s,%d,%ld,%d,%d,%d,%d,%d,%s\n",
                        r->id,
                        c->username,
                        c->role,
                        (long)c->quiz_start_time,
                        c->is_taking_quiz,
                        c->has_submitted,
                        c->score,
                        c->total,
                        c->current_question,
                        c->answers); 
            }
        }
    }

    fclose(file);
    return 0;
}

int storage_load_server_state(ServerContext* ctx) {
    if (!ctx) return -1;

    FILE* file = fopen("data/server_state.save", "r");
    if (!file) {
        LOG_INFO("No server state file found (clean start)");
        return 0; // Not an error, just clean start
    }

    char line[BUFFER_SIZE * 3]; // Large buffer for questions
    
    // Clear existing context logic just in case, but usually called on fresh init
    ctx->room_count = 0;
    ctx->client_count = 0; // We don't restore connected clients, only room logic state

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n\r")] = 0;
        if (strlen(line) == 0) continue;

        if (strncmp(line, "NEXT_ROOM_ID:", 13) == 0) {
            sscanf(line + 13, "%d", &ctx->next_room_id);
        }
        else if (strncmp(line, "ROOM:", 5) == 0) {
            if (ctx->room_count >= MAX_ROOMS) continue;
            Room* r = &ctx->rooms[ctx->room_count];
            memset(r, 0, sizeof(Room));
            
            int rand_ans = 0;
            
            // Allow sloppy parsing:
            sscanf(line + 5, "%d,%49[^,],%d,%d,%ld,%ld,%d,%d,%127[^,],%49s",
                   &r->id,
                   r->host_username,
                   &r->quiz_duration,
                   (int*)&r->state,
                   (long*)&r->start_time,
                   (long*)&r->end_time,
                   &rand_ans,
                   &r->client_count,
                   r->question_file,
                   r->correct_answers);
            
            r->randomize_answers = rand_ans;
            r->host_sock = -1; // disconnected
            r->client_count = 0; // We will increment as we load participants
            
            ctx->room_count++;
        }
        else if (strncmp(line, "ROOM_QUESTIONS:", 15) == 0) {
             // ROOM_QUESTIONS:id:content
             int rid = 0;
             char* content = strchr(line + 15, ':');
             if (content) {
                 *content = '\0';
                 rid = atoi(line + 15);
                 content++; // start of text
                 
                 // Find the room
                 for(int i=0; i<ctx->room_count; i++) {
                     if (ctx->rooms[i].id == rid) {
                         // Unescape [NL] -> \n
                         char* src = content;
                         char* dst = ctx->rooms[i].questions;
                         while(*src) {
                            if (strncmp(src, "[NL]", 4) == 0) {
                                *dst++ = '\n';
                                src += 4;
                            } else {
                                *dst++ = *src++;
                            }
                         }
                         *dst = '\0';
                         break;
                     }
                 }
             }
        }
        else if (strncmp(line, "PARTICIPANT:", 12) == 0) {
            // PARTICIPANT:roomid,user,role,start,taking,submitted,score,total,curr,answers
            int rid=0, role=0, taking=0, sub=0, sc=0, tot=0, cur=0;
            long start=0;
            char user[50], ans[50];
            strcpy(ans, ""); // default empty
            
            int parsed = sscanf(line + 12, "%d,%49[^,],%d,%ld,%d,%d,%d,%d,%d,%49s",
                   &rid, user, &role, &start, &taking, &sub, &sc, &tot, &cur, ans);

            if (parsed >= 9) { // answers can be empty/missing
               for(int i=0; i<ctx->room_count; i++) {
                   if (ctx->rooms[i].id == rid) {
                       Room* r = &ctx->rooms[i];
                       if (r->client_count < MAX_CLIENTS && ctx->client_count < MAX_CLIENTS) {
                           
                           Client* c = &ctx->clients[ctx->client_count];
                           memset(c, 0, sizeof(Client));
                           c->sock = -1; // disconnected
                           strncpy(c->username, user, sizeof(c->username)-1);
                           c->role = role;
                           c->quiz_start_time = (time_t)start;
                           c->is_taking_quiz = taking;
                           c->has_submitted = sub;
                           c->score = sc;
                           c->total = tot;
                           c->current_question = cur;
                           strncpy(c->answers, ans, sizeof(c->answers)-1);
                           
                           // Add to room
                           r->clients[r->client_count] = c;
                           r->client_count++;
                           
                           ctx->client_count++;
                       }
                       break;
                   }
               }
            }
        }
    }

    fclose(file);
    LOG_INFO("Server state loaded successfully");
    char msg[64];
    snprintf(msg, sizeof(msg), "Restored %d rooms", ctx->room_count);
    LOG_INFO(msg);
    return 0;
}
