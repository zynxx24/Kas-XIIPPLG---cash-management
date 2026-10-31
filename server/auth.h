/*
 * XII PPLG Cash Management - Authentication Module
 */
#ifndef AUTH_H
#define AUTH_H

#include "cJSON.h"

#define MAX_SESSIONS 64
#define TOKEN_LEN    64

typedef struct {
    char token[TOKEN_LEN + 1];
    char username[256];
    char role[16]; /* "admin" or "user" */
    int  member_id;
    long expires;
} Session;

/* Initialize auth module, loads credentials from .ENV */
void auth_init(void);

/* Get admin credentials loaded from .ENV */
const char *auth_get_admin_username(void);
const char *auth_get_admin_password_hash(void);
const char *auth_get_backup_key(void);

/* Login: returns token string or NULL on failure */
char *auth_login(const char *username, const char *password, const char *role, int member_id);

/* Validate token: returns session pointer or NULL */
Session *auth_validate(const char *token);

/* Logout */
void auth_logout(const char *token);

/* Cleanup expired sessions */
void auth_cleanup(void);

#endif /* AUTH_H */
