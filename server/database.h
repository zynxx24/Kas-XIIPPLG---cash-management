/*
 * XII PPLG Cash Management - Database Module
 * JSON file-based storage with thread-safe operations
 */
#ifndef DATABASE_H
#define DATABASE_H

#include "cJSON.h"

#define DATA_DIR "../data"

/* Initialize database and create default data files */
void db_init(void);

/* Generic file operations */
cJSON *db_read_file(const char *filename);
int    db_write_file(const char *filename, cJSON *data);

/* Members */
cJSON *db_get_members(void);

/* Payments */
cJSON *db_get_payments(void);
int    db_save_payments(cJSON *payments);

/* Donations */
cJSON *db_get_donations(void);
int    db_save_donations(cJSON *donations);

/* Expenses */
cJSON *db_get_expenses(void);
int    db_save_expenses(cJSON *expenses);

/* Config */
cJSON *db_get_config(void);
int    db_save_config(cJSON *config);

/* Users */
cJSON *db_get_users(void);
int    db_save_users(cJSON *users);

/* 2-Way Encrypted Auto-Backup */
int    db_create_backup(void);
void   db_start_backup_timer(void);

#endif /* DATABASE_H */
