#ifndef STORAGE_H
#define STORAGE_H

void storage_init();
int storage_load_users(const char *filename);
int storage_check_credentials(const char *username, const char *password);
int storage_add_user(const char *username, const char *password);
int storage_user_exists(const char *username);

#endif
