#include "storage.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "cJSON.h"

#define MAX_USERS 100
#define MAX_NAME_LEN 32
#define MAX_PASS_LEN 32
#define MAX_ROLE_LEN 16
#define MAX_LINE_LEN 128
#define DEFAUlT_USER_FILE "data/users.txt"
#define ROOMS_FILE "data/rooms.json"
#define QUESTION_BANK_DIR "data/questions/"

typedef struct {
  char username[MAX_NAME_LEN];
  char password[MAX_PASS_LEN];
  char role[MAX_ROLE_LEN];
} User;

static User users[MAX_USERS];
static int user_count = 0;
static const char* user_file_path = DEFAUlT_USER_FILE;

void storage_init() {
  user_count = 0;

  // Ensure directories exist
  mkdir("data", 0777);
  mkdir(QUESTION_BANK_DIR, 0777);

  if (storage_load_users(user_file_path) < 0) {
    printf("Failed to load users from %s\n", user_file_path);
  } else {
    printf("Loaded %d users.\n", user_count);
  }
}

int storage_load_users(const char* filename) {
  FILE* f = fopen(filename, "r");
  if (!f)
    return -1;

  char line[MAX_LINE_LEN];
  while (fgets(line, sizeof(line), f)) {
    if (user_count >= MAX_USERS)
      break;

    // Remove newline
    line[strcspn(line, "\r\n")] = 0;

    char* user = strtok(line, ":");
    char* pass = strtok(NULL, ":");
    char* role = strtok(NULL, ":");

    if (user && pass) {
      strncpy(users[user_count].username, user, MAX_NAME_LEN - 1);
      users[user_count].username[MAX_NAME_LEN - 1] = '\0';

      strncpy(users[user_count].password, pass, MAX_PASS_LEN - 1);
      users[user_count].password[MAX_PASS_LEN - 1] = '\0';

      if (role) {
        strncpy(users[user_count].role, role, MAX_ROLE_LEN - 1);
        users[user_count].role[MAX_ROLE_LEN - 1] = '\0';
      } else {
        strcpy(users[user_count].role, "participant");
      }

      user_count++;
    }
  }
  fclose(f);
  return 0;
}

int storage_check_credentials(const char* username, const char* password) {
  for (int i = 0; i < user_count; i++) {
    if (strcmp(users[i].username, username) == 0 &&
        strcmp(users[i].password, password) == 0) {
      return 1;
    }
  }
  return 0;
}

int storage_user_exists(const char* username) {
  for (int i = 0; i < user_count; i++) {
    if (strcmp(users[i].username, username) == 0) {
      return 1;
    }
  }
  return 0;
}

const char* storage_get_role(const char* username) {
  for (int i = 0; i < user_count; i++) {
    if (strcmp(users[i].username, username) == 0) {
      return users[i].role;
    }
  }
  return "participant";
}

int storage_add_user(const char* username,
                     const char* password,
                     const char* role) {
  if (user_count >= MAX_USERS)
    return -1;
  if (storage_user_exists(username))
    return -2;

  strncpy(users[user_count].username, username, MAX_NAME_LEN - 1);
  users[user_count].username[MAX_NAME_LEN - 1] = '\0';

  strncpy(users[user_count].password, password, MAX_PASS_LEN - 1);
  users[user_count].password[MAX_PASS_LEN - 1] = '\0';

  if (role) {
    strncpy(users[user_count].role, role, MAX_ROLE_LEN - 1);
    users[user_count].role[MAX_ROLE_LEN - 1] = '\0';
  } else {
    strcpy(users[user_count].role, "participant");
  }

  user_count++;

  // Append to file
  FILE* f = fopen(user_file_path, "a");
  if (f) {
    fprintf(f, "%s:%s:%s\n", username, password, users[user_count - 1].role);
    fclose(f);
    return 0;
  }
  return -3;
}

// Room Management
int storage_save_room(const Room* room) {
  cJSON* root = NULL;
  FILE* f = fopen(ROOMS_FILE, "r");
  if (f) {
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    root = cJSON_Parse(data);
    free(data);
  }

  if (!root) {
    root = cJSON_CreateArray();
  }

  cJSON* room_obj = cJSON_CreateObject();
  cJSON_AddStringToObject(room_obj, "id", room->id);
  cJSON_AddStringToObject(room_obj, "name", room->name);
  cJSON_AddNumberToObject(room_obj, "start_time", room->start_time);
  cJSON_AddNumberToObject(room_obj, "end_time", room->end_time);
  cJSON_AddStringToObject(room_obj, "question_bank_id", room->question_bank_id);
  cJSON_AddStringToObject(room_obj, "status", room->status);
  cJSON_AddNumberToObject(room_obj, "num_questions", room->num_questions);
  cJSON_AddNumberToObject(room_obj, "allowed_attempts", room->allowed_attempts);

  cJSON_AddItemToArray(root, room_obj);

  char* json_str = cJSON_Print(root);
  f = fopen(ROOMS_FILE, "w");
  if (f) {
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    cJSON_Delete(root);
    return 0;
  }

  free(json_str);
  cJSON_Delete(root);
  return -1;
}

int storage_get_rooms(cJSON* rooms_array) {
  FILE* f = fopen(ROOMS_FILE, "r");
  if (!f)
    return 0;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* data = malloc(len + 1);
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);

  cJSON* root = cJSON_Parse(data);
  free(data);

  if (root && cJSON_IsArray(root)) {
    cJSON* item = NULL;
    cJSON_ArrayForEach(item, root) {
      cJSON_AddItemToArray(rooms_array, cJSON_Duplicate(item, 1));
    }
  }

  cJSON_Delete(root);
  return 0;
}

// Question Management
int storage_save_question_bank(const char* bank_name, cJSON* questions) {
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s%s.json", QUESTION_BANK_DIR,
           bank_name);

  char* json_str = cJSON_Print(questions);
  FILE* f = fopen(filepath, "w");
  if (f) {
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    return 0;
  }
  free(json_str);
  return -1;
}

int storage_list_question_banks(cJSON* banks_array) {
  DIR* d;
  struct dirent* dir;
  d = opendir(QUESTION_BANK_DIR);
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (strstr(dir->d_name, ".json")) {
        char bank_id[256];
        strncpy(bank_id, dir->d_name, sizeof(bank_id));
        // remove extension
        char* ext = strstr(bank_id, ".json");
        if (ext)
          *ext = '\0';

        cJSON_AddItemToArray(banks_array, cJSON_CreateString(bank_id));
      }
    }
    closedir(d);
    return 0;
  }
  return -1;
}

int storage_get_question_bank(const char* bank_id, cJSON** questions) {
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s%s.json", QUESTION_BANK_DIR, bank_id);

  FILE* f = fopen(filepath, "r");
  if (!f)
    return -1;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* data = malloc(len + 1);
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);

  *questions = cJSON_Parse(data);
  free(data);

  return (*questions) ? 0 : -1;
}

int storage_update_question_bank(const char* bank_id, cJSON* questions) {
  return storage_save_question_bank(bank_id, questions);
}

int storage_delete_question_bank(const char* bank_id) {
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s%s.json", QUESTION_BANK_DIR, bank_id);
  if (remove(filepath) == 0) {
    return 0;
  }
  return -1;
}

// Room updates
int storage_update_room_status(const char* room_id, const char* status) {
  // Naive implementation: Load all, find, update, save all.
  // Ideally use database or individual files.
  cJSON* root = NULL;
  FILE* f = fopen(ROOMS_FILE, "r");
  if (!f)
    return -1;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* data = malloc(len + 1);
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);
  root = cJSON_Parse(data);
  free(data);

  if (!root)
    return -1;

  int found = 0;
  cJSON* item = NULL;
  cJSON_ArrayForEach(item, root) {
    cJSON* id = cJSON_GetObjectItem(item, "id");
    if (id && strcmp(id->valuestring, room_id) == 0) {
      cJSON_ReplaceItemInObject(item, "status", cJSON_CreateString(status));
      found = 1;
      break;
    }
  }

  if (found) {
    char* json_str = cJSON_Print(root);
    f = fopen(ROOMS_FILE, "w");
    if (f) {
      fprintf(f, "%s", json_str);
      fclose(f);
      free(json_str);
      cJSON_Delete(root);
      return 0;
    }
    free(json_str);
  }
  cJSON_Delete(root);
  return -1;
}

int storage_delete_room(const char* room_id) {
  cJSON* root = NULL;
  FILE* f = fopen(ROOMS_FILE, "r");
  if (!f)
    return -1;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* data = malloc(len + 1);
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);
  root = cJSON_Parse(data);
  free(data);

  if (!root)
    return -1;

  cJSON* new_root = cJSON_CreateArray();
  int found = 0;
  cJSON* item = NULL;
  cJSON_ArrayForEach(item, root) {
    cJSON* id = cJSON_GetObjectItem(item, "id");
    if (id && strcmp(id->valuestring, room_id) == 0) {
      found = 1;
      // Skip adding to new root
    } else {
      cJSON_AddItemToArray(new_root, cJSON_Duplicate(item, 1));
    }
  }

  cJSON_Delete(root);

  if (found) {
    char* json_str = cJSON_Print(new_root);
    f = fopen(ROOMS_FILE, "w");
    if (f) {
      fprintf(f, "%s", json_str);
      fclose(f);
      free(json_str);
      cJSON_Delete(new_root);
      return 0;
    }
    free(json_str);
  }
  cJSON_Delete(new_root);
  return -1;
}

// Result Management
#define RESULT_DIR "data/results/"

int storage_save_result(const RoomResult* result) {
  mkdir(RESULT_DIR, 0777);
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s%s.json", RESULT_DIR,
           result->room_id);

  // Results per room are stored in a single JSON array file
  cJSON* root = NULL;
  FILE* f = fopen(filepath, "r");
  if (f) {
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);
    root = cJSON_Parse(data);
    free(data);
  }

  if (!root) {
    root = cJSON_CreateArray();
  }

  cJSON* res_obj = cJSON_CreateObject();
  cJSON_AddStringToObject(res_obj, "username", result->username);
  cJSON_AddNumberToObject(res_obj, "score", result->score);
  cJSON_AddNumberToObject(res_obj, "timestamp", result->timestamp);

  cJSON_AddItemToArray(root, res_obj);

  char* json_str = cJSON_Print(root);
  f = fopen(filepath, "w");
  if (f) {
    fprintf(f, "%s", json_str);
    fclose(f);
    free(json_str);
    cJSON_Delete(root);
    return 0;
  }
  free(json_str);
  cJSON_Delete(root);
  return -1;
}

int storage_get_room_results(const char* room_id, cJSON* results_array) {
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "%s%s.json", RESULT_DIR, room_id);

  FILE* f = fopen(filepath, "r");
  if (!f)
    return 0;  // No results is fine

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* data = malloc(len + 1);
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);

  cJSON* root = cJSON_Parse(data);
  free(data);

  if (root && cJSON_IsArray(root)) {
    cJSON* item;
    cJSON_ArrayForEach(item, root) {
      cJSON_AddItemToArray(results_array, cJSON_Duplicate(item, 1));
    }
  }
  cJSON_Delete(root);
  return 0;
}

cJSON* storage_get_room(const char* room_id) {
  FILE* f = fopen(ROOMS_FILE, "r");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* data = malloc(len + 1);
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);

  cJSON* root = cJSON_Parse(data);
  free(data);

  if (!root)
    return NULL;

  cJSON* target = NULL;
  cJSON* item = NULL;
  cJSON_ArrayForEach(item, root) {
    cJSON* id = cJSON_GetObjectItem(item, "id");
    if (id && strcmp(id->valuestring, room_id) == 0) {
      target = cJSON_Duplicate(item, 1);
      break;
    }
  }
  cJSON_Delete(root);
  return target;
}
