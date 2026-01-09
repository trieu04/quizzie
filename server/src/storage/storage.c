#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERS 100
#define MAX_NAME_LEN 32
#define MAX_PASS_LEN 32
#define MAX_LINE_LEN 128
#define DEFAUlT_USER_FILE "data/users.txt"

typedef struct {
    char username[MAX_NAME_LEN];
    char password[MAX_PASS_LEN];
} User;

static User users[MAX_USERS];
static int user_count = 0;
static const char *user_file_path = DEFAUlT_USER_FILE;

void storage_init() {
    user_count = 0;
    if (storage_load_users(user_file_path) < 0) {
        printf("Failed to load users from %s\n", user_file_path);
    } else {
        printf("Loaded %d users.\n", user_count);
    }
}

int storage_load_users(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        if (user_count >= MAX_USERS) break;
        
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;
        
        char *user = strtok(line, ":");
        char *pass = strtok(NULL, ":");
        
        if (user && pass) {
            strncpy(users[user_count].username, user, MAX_NAME_LEN - 1);
            users[user_count].username[MAX_NAME_LEN - 1] = '\0';
            
            strncpy(users[user_count].password, pass, MAX_PASS_LEN - 1);
            users[user_count].password[MAX_PASS_LEN - 1] = '\0';

            user_count++;
        }
    }
    fclose(f);
    return 0;
}

int storage_check_credentials(const char *username, const char *password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && 
            strcmp(users[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
}

int storage_user_exists(const char *username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return 1;
        }
    }
    return 0;
}

int storage_add_user(const char *username, const char *password) {
    if (user_count >= MAX_USERS) return -1;
    if (storage_user_exists(username)) return -2;

    strncpy(users[user_count].username, username, MAX_NAME_LEN - 1);
    users[user_count].username[MAX_NAME_LEN - 1] = '\0';
    
    strncpy(users[user_count].password, password, MAX_PASS_LEN - 1);
    users[user_count].password[MAX_PASS_LEN - 1] = '\0';
    
    user_count++;

    // Append to file
    FILE *f = fopen(user_file_path, "a");
    if (f) {
        fprintf(f, "%s:%s:participant\n", username, password);
        fclose(f);
        return 0;
    }
    return -3;
}
