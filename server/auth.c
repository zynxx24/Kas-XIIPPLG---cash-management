/*
 * XII PPLG Cash Management - Authentication Module
 */
#include "auth.h"
#include "database.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

static Session sessions[MAX_SESSIONS];
static pthread_mutex_t auth_mutex = PTHREAD_MUTEX_INITIALIZER;

static char admin_username[256] = "admin";
static char admin_password_hash[65] = "240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9";
static char backup_key[256] = "XII_PPLG_DEFAULT_BACKUP_KEY_2026";

const char *auth_get_admin_username(void) {
    return admin_username;
}

const char *auth_get_admin_password_hash(void) {
    return admin_password_hash;
}

const char *auth_get_backup_key(void) {
    return backup_key;
}

static void load_env(void) {
    const char *paths[] = {
        ".ENV",
        "server/.ENV",
        "/home/wira/Desktop/XIIPPLG/server/.ENV",
        ".env",
        "server/.env"
    };
    
    FILE *f = NULL;
    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        f = fopen(paths[i], "r");
        if (f) {
            printf("[AUTH] Found config at: %s\n", paths[i]);
            break;
        }
    }

    if (!f) {
        /* Auto-create .ENV in server directory if missing */
        f = fopen(".ENV", "w");
        if (!f) f = fopen("server/.ENV", "w");
        if (!f) f = fopen("/home/wira/Desktop/XIIPPLG/server/.ENV", "w");
        if (f) {
            fprintf(f, "# XII PPLG Cash Management Configuration\n");
            fprintf(f, "ADMIN_USERNAME=admin\n");
            fprintf(f, "ADMIN_PASSWORD=admin123\n");
            fclose(f);
            printf("[AUTH] Created default .ENV configuration\n");
        }
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || line[0] == 0) continue;
        
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = 0;
            char *key = line;
            char *val = eq + 1;
            
            while (*key == ' ' || *key == '\t') key++;
            char *end = key + strlen(key) - 1;
            while (end > key && (*end == ' ' || *end == '\t')) { *end = 0; end--; }
            
            while (*val == ' ' || *val == '\t') val++;
            end = val + strlen(val) - 1;
            while (end > val && (*end == ' ' || *end == '\t')) { *end = 0; end--; }
            
            if (strcmp(key, "ADMIN_USERNAME") == 0) {
                strncpy(admin_username, val, sizeof(admin_username) - 1);
            } else if (strcmp(key, "ADMIN_PASSWORD") == 0) {
                sha256_hash_string(val, admin_password_hash);
            } else if (strcmp(key, "ADMIN_PASSWORD_HASH") == 0) {
                strncpy(admin_password_hash, val, sizeof(admin_password_hash) - 1);
            } else if (strcmp(key, "BACKUP_KEY") == 0) {
                strncpy(backup_key, val, sizeof(backup_key) - 1);
            }
        }
    }
    fclose(f);
    printf("[AUTH] Loaded Admin Username: %s\n", admin_username);
    printf("[AUTH] Loaded Admin PW Hash:  %s\n", admin_password_hash);
}

void auth_init(void) {
    memset(sessions, 0, sizeof(sessions));
    load_env();
    printf("[AUTH] Authentication module initialized\n");
}

static void generate_token(char *token) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(NULL) ^ (unsigned long)pthread_self());
    for (int i = 0; i < TOKEN_LEN; i++) {
        token[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    token[TOKEN_LEN] = 0;
}

char *auth_login(const char *username, const char *password, const char *role, int member_id) {
    /* For admin login, verify credentials from .ENV using SHA-256 */
    if (strcmp(role, "admin") == 0) {
        char input_hash[65];
        sha256_hash_string(password, input_hash);
        
        if (strcmp(username, admin_username) != 0 || strcmp(input_hash, admin_password_hash) != 0) {
            return NULL;
        }
    }
    /* For user login, just verify member_id exists */
    else if (strcmp(role, "user") == 0) {
        cJSON *members = db_get_members();
        if (!members) return NULL;
        int found = 0;
        int size = cJSON_GetArraySize(members);
        for (int i = 0; i < size; i++) {
            cJSON *m = cJSON_GetArrayItem(members, i);
            cJSON *id = cJSON_GetObjectItem(m, "id");
            if (id && id->valueint == member_id) {
                found = 1;
                break;
            }
        }
        cJSON_Delete(members);
        if (!found) return NULL;
    }

    pthread_mutex_lock(&auth_mutex);

    /* Find empty session slot */
    int slot = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].token[0] == 0 || sessions[i].expires < time(NULL)) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        /* Replace oldest session */
        long oldest = sessions[0].expires;
        slot = 0;
        for (int i = 1; i < MAX_SESSIONS; i++) {
            if (sessions[i].expires < oldest) {
                oldest = sessions[i].expires;
                slot = i;
            }
        }
    }

    generate_token(sessions[slot].token);
    strncpy(sessions[slot].username, username, 255);
    strncpy(sessions[slot].role, role, 15);
    sessions[slot].member_id = member_id;
    sessions[slot].expires = time(NULL) + 86400; /* 24 hours */

    char *token = strdup(sessions[slot].token);
    pthread_mutex_unlock(&auth_mutex);

    printf("[AUTH] Login: %s (role: %s)\n", username, role);
    return token;
}

Session *auth_validate(const char *token) {
    if (!token || !token[0]) return NULL;

    pthread_mutex_lock(&auth_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].token[0] && strcmp(sessions[i].token, token) == 0) {
            if (sessions[i].expires > time(NULL)) {
                pthread_mutex_unlock(&auth_mutex);
                return &sessions[i];
            }
            /* Expired - clear it */
            memset(&sessions[i], 0, sizeof(Session));
            break;
        }
    }
    pthread_mutex_unlock(&auth_mutex);
    return NULL;
}

void auth_logout(const char *token) {
    if (!token) return;
    pthread_mutex_lock(&auth_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].token[0] && strcmp(sessions[i].token, token) == 0) {
            printf("[AUTH] Logout: %s\n", sessions[i].username);
            memset(&sessions[i], 0, sizeof(Session));
            break;
        }
    }
    pthread_mutex_unlock(&auth_mutex);
}

void auth_cleanup(void) {
    long now = time(NULL);
    pthread_mutex_lock(&auth_mutex);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].token[0] && sessions[i].expires < now) {
            memset(&sessions[i], 0, sizeof(Session));
        }
    }
    pthread_mutex_unlock(&auth_mutex);
}
