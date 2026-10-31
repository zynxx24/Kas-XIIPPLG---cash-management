/*
 * cJSON - Minimal JSON parser for XII PPLG Cash Management
 * Simplified implementation for embedded use
 */
#ifndef CJSON_H
#define CJSON_H

#include <stddef.h>

/* cJSON Types */
#define cJSON_Invalid  0
#define cJSON_False    1
#define cJSON_True     2
#define cJSON_NULL     3
#define cJSON_Number   4
#define cJSON_String   5
#define cJSON_Array    6
#define cJSON_Object   7

typedef struct cJSON {
    struct cJSON *next, *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    double valuedouble;
    int valueint;
    char *string; /* key name */
} cJSON;

/* Parsing */
cJSON *cJSON_Parse(const char *value);
void cJSON_Delete(cJSON *item);

/* Printing */
char *cJSON_Print(const cJSON *item);
char *cJSON_PrintUnformatted(const cJSON *item);

/* Creation */
cJSON *cJSON_CreateObject(void);
cJSON *cJSON_CreateArray(void);
cJSON *cJSON_CreateString(const char *string);
cJSON *cJSON_CreateNumber(double num);
cJSON *cJSON_CreateBool(int boolean);
cJSON *cJSON_CreateNull(void);
cJSON *cJSON_CreateTrue(void);
cJSON *cJSON_CreateFalse(void);

/* Object operations */
void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
void cJSON_AddItemToArray(cJSON *array, cJSON *item);
cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
void cJSON_DeleteItemFromObject(cJSON *object, const char *string);

/* Array operations */
int cJSON_GetArraySize(const cJSON *array);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);

/* Convenience macros */
#define cJSON_AddStringToObject(obj, key, val) \
    cJSON_AddItemToObject(obj, key, cJSON_CreateString(val))
#define cJSON_AddNumberToObject(obj, key, val) \
    cJSON_AddItemToObject(obj, key, cJSON_CreateNumber(val))
#define cJSON_AddBoolToObject(obj, key, val) \
    cJSON_AddItemToObject(obj, key, cJSON_CreateBool(val))

/* Utilities */
int cJSON_IsTrue(const cJSON *item);
int cJSON_IsFalse(const cJSON *item);
int cJSON_IsNull(const cJSON *item);
int cJSON_IsNumber(const cJSON *item);
int cJSON_IsString(const cJSON *item);
int cJSON_IsArray(const cJSON *item);
int cJSON_IsObject(const cJSON *item);
cJSON *cJSON_Duplicate(const cJSON *item, int recurse);

#endif /* CJSON_H */
