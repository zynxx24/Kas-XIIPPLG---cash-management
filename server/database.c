/*
 * XII PPLG Cash Management - Database Module
 */
#include "database.h"
#include "auth.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *MEMBER_NAMES[] = {
    "BOYKE VILANO HAMONANGAN SIHITE",
    "BINTANG LEONITA CHRISTYA RENATA",
    "CAROLINA TIMUTHY JANGGUR",
    "DEWA GEDE DALEM OKA ADNYANA SANDI",
    "GALISTAN RAMADHAN KURNIA TAUNAES",
    "GEDE AGUS WIRA DARMA PUTRA",
    "I GEDE ABI WIRYA DINATA",
    "I GEDE DARMA SUPTIAWAN",
    "I KOMANG RADITYA PUTRA",
    "I KOMANG RISKI SETIAWAN",
    "I NYOMAN GEDE ARTA WIGUNA",
    "I PUTU DIKA LAKSMANA PUTRA",
    "I PUTU DITYA ARTHA WIJAYA",
    "I PUTU PANDE ANDIKA",
    "I PUTU SUYOGA MAHENDRA",
    "I WAYAN BAGUS PUTRAWAN",
    "I WAYAN PASEK KEVIN ARIADI",
    "KADEK YUDA PRASETYA",
    "KADEK YUNI CALLISTA PUTRI DEWI",
    "KOMANG DIAH PUTRI PRATIWI",
    "LUH RIA MIRASIH",
    "NI KADEK ADELIA CAHYA KENCANA PUTRI",
    "NI KADEK LINA ANTIKA DEWI",
    "NI KOMANG KIRANA PARAMITA ARDANARI",
    "NI KOMANG SEPTIARINI",
    "NI LUH PUTU KESYA ASTRI MELANI",
    "NI PUTU CAHAYA LESTARI DEWI",
    "NI PUTU INTAN LESTARI DARMAYANTI",
    "OKTA PRADIPTA ATTALA DZAKI",
    "PUTU BAYU SATRIA WANGSA BUKIAN",
    "PUTU NANDA LINDIA MAHARANI",
    "PUTU PUTRI CAHYANI",
    "RADITYA RONDI"
};
static const int NUM_MEMBERS = 33;

static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

cJSON *db_read_file(const char *filename) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", DATA_DIR, filename);

    pthread_mutex_lock(&db_mutex);
    FILE *f = fopen(filepath, "r");
    if (!f) {
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);
    pthread_mutex_unlock(&db_mutex);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json;
}

int db_write_file(const char *filename, cJSON *data) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", DATA_DIR, filename);

    char *str = cJSON_Print(data);
    if (!str) return -1;

    pthread_mutex_lock(&db_mutex);
    FILE *f = fopen(filepath, "w");
    if (!f) {
        pthread_mutex_unlock(&db_mutex);
        free(str);
        return -1;
    }

    fputs(str, f);
    fclose(f);
    pthread_mutex_unlock(&db_mutex);

    free(str);
    return 0;
}

static void init_members(void) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/members.json", DATA_DIR);
    
    struct stat st;
    if (stat(filepath, &st) == 0) return; /* already exists */

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < NUM_MEMBERS; i++) {
        cJSON *member = cJSON_CreateObject();
        cJSON_AddNumberToObject(member, "id", i + 1);
        cJSON_AddStringToObject(member, "name", MEMBER_NAMES[i]);
        cJSON_AddItemToArray(arr, member);
    }
    db_write_file("members.json", arr);
    cJSON_Delete(arr);
}

static void init_payments(void) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/payments.json", DATA_DIR);

    struct stat st;
    if (stat(filepath, &st) == 0) return;

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "dates", cJSON_CreateArray());
    cJSON_AddItemToObject(obj, "records", cJSON_CreateObject());
    db_write_file("payments.json", obj);
    cJSON_Delete(obj);
}

static void init_donations(void) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/donations.json", DATA_DIR);

    struct stat st;
    if (stat(filepath, &st) == 0) return;

    cJSON *arr = cJSON_CreateArray();
    db_write_file("donations.json", arr);
    cJSON_Delete(arr);
}

static void init_config(void) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/config.json", DATA_DIR);

    struct stat st;
    if (stat(filepath, &st) == 0) return;

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "kas_amount", 5000);
    cJSON_AddNumberToObject(obj, "previous_balance", 0);
    cJSON_AddStringToObject(obj, "class_name", "XII PPLG");
    cJSON_AddStringToObject(obj, "year", "2026/2027");
    db_write_file("config.json", obj);
    cJSON_Delete(obj);
}

static void init_expenses(void) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/expenses.json", DATA_DIR);

    struct stat st;
    if (stat(filepath, &st) == 0) return;

    cJSON *arr = cJSON_CreateArray();
    db_write_file("expenses.json", arr);
    cJSON_Delete(arr);
}

static void init_users(void) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/users.json", DATA_DIR);

    struct stat st;
    if (stat(filepath, &st) == 0) return;

    cJSON *obj = cJSON_CreateObject();
    cJSON *admin = cJSON_CreateObject();
    cJSON_AddStringToObject(admin, "password", "admin123");
    cJSON_AddStringToObject(admin, "role", "admin");
    cJSON_AddItemToObject(obj, "admin", admin);
    db_write_file("users.json", obj);
    cJSON_Delete(obj);
}

void db_init(void) {
    ensure_dir(DATA_DIR);
    init_members();
    init_payments();
    init_donations();
    init_expenses();
    init_config();
    init_users();
    printf("[DATABASE] Initialized with %d members\n", NUM_MEMBERS);
}

cJSON *db_get_members(void)   { return db_read_file("members.json"); }
cJSON *db_get_payments(void)  { return db_read_file("payments.json"); }
cJSON *db_get_donations(void) { return db_read_file("donations.json"); }
cJSON *db_get_expenses(void)  { return db_read_file("expenses.json"); }
cJSON *db_get_config(void)    { return db_read_file("config.json"); }
cJSON *db_get_users(void)     { return db_read_file("users.json"); }

int db_save_payments(cJSON *data)  { return db_write_file("payments.json", data); }
int db_save_donations(cJSON *data) { return db_write_file("donations.json", data); }
int db_save_expenses(cJSON *data)  { return db_write_file("expenses.json", data); }
int db_save_config(cJSON *data)    { return db_write_file("config.json", data); }
int db_save_users(cJSON *data)     { return db_write_file("users.json", data); }

/* ---- 2-Way Encrypted Auto-Backup ---- */

/* Reversible 2-way SHA-256 CTR keystream stream cipher */
static void encrypt_2way_stream(const uint8_t *input, uint8_t *output, size_t len, const char *key) {
    size_t key_len = strlen(key);
    uint32_t block_counter = 0;
    uint8_t ks[32];

    for (size_t i = 0; i < len; i++) {
        size_t block_offset = i % 32;
        if (block_offset == 0) {
            SHA256_CTX ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, (const uint8_t*)key, key_len);
            sha256_update(&ctx, (const uint8_t*)&block_counter, sizeof(block_counter));
            sha256_final(&ctx, ks);
            block_counter++;
        }
        output[i] = input[i] ^ ks[block_offset];
    }
}

static void add_json_to_bundle(cJSON *bundle, const char *key, cJSON *val) {
    if (!val) val = cJSON_CreateArray();
    cJSON_AddItemToObject(bundle, key, val);
}

int db_create_backup(void) {
    const char *data_dirs[] = {
        DATA_DIR,
        "data",
        "../data",
        "/home/wira/Desktop/XIIPPLG/data"
    };

    char backup_dir[512] = {0};
    for (size_t i = 0; i < sizeof(data_dirs)/sizeof(data_dirs[0]); i++) {
        struct stat st;
        if (stat(data_dirs[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(backup_dir, sizeof(backup_dir), "%s/backups", data_dirs[i]);
            break;
        }
    }

    if (backup_dir[0] == 0) {
        snprintf(backup_dir, sizeof(backup_dir), "%s/backups", DATA_DIR);
    }
    ensure_dir(backup_dir);

    cJSON *bundle = cJSON_CreateObject();
    add_json_to_bundle(bundle, "members",   db_get_members());
    add_json_to_bundle(bundle, "payments",  db_get_payments());
    add_json_to_bundle(bundle, "donations", db_get_donations());
    add_json_to_bundle(bundle, "expenses",  db_get_expenses());
    add_json_to_bundle(bundle, "config",    db_get_config());
    add_json_to_bundle(bundle, "users",     db_get_users());

    char *json_str = cJSON_PrintUnformatted(bundle);
    cJSON_Delete(bundle);

    if (!json_str) {
        fprintf(stderr, "[BACKUP] Failed to serialize database for backup\n");
        return -1;
    }

    size_t len = strlen(json_str);
    uint8_t *encrypted = malloc(len ? len : 1);
    if (!encrypted) {
        free(json_str);
        return -1;
    }

    const char *key = auth_get_backup_key();
    encrypt_2way_stream((const uint8_t*)json_str, encrypted, len, key);
    free(json_str);

    /* Timestamp YYYYMMDD_HHMMSS */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/backup_%04d%02d%02d_%02d%02d%02d.enc",
             backup_dir,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "[BACKUP] Failed to open backup file for writing: %s\n", filename);
        free(encrypted);
        return -1;
    }

    /* Write 16-byte magic header "XII_PPLG_ENC_V1\n" */
    fwrite("XII_PPLG_ENC_V1\n", 1, 16, f);
    fwrite(encrypted, 1, len, f);
    fclose(f);
    free(encrypted);

    printf("[BACKUP] Successfully created 2-way encrypted backup: %s (%zu bytes)\n", filename, len);
    return 0;
}

static void *backup_timer_thread(void *arg) {
    (void)arg;
    /* 8 hours loop: 8 * 3600 = 28800 seconds */
    const int interval_sec = 8 * 3600;
    while (1) {
        sleep(interval_sec);
        printf("[BACKUP] Running scheduled 8-hour database backup...\n");
        db_create_backup();
    }
    return NULL;
}

void db_start_backup_timer(void) {
    /* Perform immediate startup backup synchronously */
    printf("[BACKUP] Creating initial database backup on server startup...\n");
    db_create_backup();

    pthread_t thread;
    if (pthread_create(&thread, NULL, backup_timer_thread, NULL) == 0) {
        pthread_detach(thread);
        printf("[BACKUP] 8-Hour Encrypted Auto-Backup service started\n");
    } else {
        fprintf(stderr, "[BACKUP] Failed to start backup timer thread\n");
    }
}
