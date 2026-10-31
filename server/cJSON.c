/*
 * cJSON - Minimal JSON parser for XII PPLG Cash Management
 */
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static cJSON *cJSON_New_Item(void) {
    cJSON *node = (cJSON*)calloc(1, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON *item) {
    cJSON *next;
    while (item) {
        next = item->next;
        if (item->child) cJSON_Delete(item->child);
        if (item->valuestring) free(item->valuestring);
        if (item->string) free(item->string);
        free(item);
        item = next;
    }
}

static char *cJSON_strdup(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = (char*)malloc(len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

/* ---- Parser ---- */
static const char *skip_whitespace(const char *in) {
    while (in && *in && (unsigned char)*in <= 32) in++;
    return in;
}

static const char *parse_string(cJSON *item, const char *str) {
    const char *ptr = str + 1;
    char *out;
    int len = 0;
    const char *ptr2 = ptr;

    if (*str != '\"') return NULL;

    while (*ptr2 != '\"' && *ptr2) {
        if (*ptr2 == '\\') ptr2++;
        ptr2++;
        len++;
    }

    out = (char*)malloc(len + 1);
    if (!out) return NULL;

    ptr2 = ptr;
    char *out2 = out;
    while (*ptr2 != '\"' && *ptr2) {
        if (*ptr2 == '\\') {
            ptr2++;
            switch (*ptr2) {
                case 'n': *out2++ = '\n'; break;
                case 't': *out2++ = '\t'; break;
                case 'r': *out2++ = '\r'; break;
                case '\\': *out2++ = '\\'; break;
                case '\"': *out2++ = '\"'; break;
                case '/': *out2++ = '/'; break;
                default: *out2++ = *ptr2; break;
            }
        } else {
            *out2++ = *ptr2;
        }
        ptr2++;
    }
    *out2 = 0;

    item->type = cJSON_String;
    item->valuestring = out;
    return ptr2 + 1;
}

static const char *parse_number(cJSON *item, const char *num) {
    double n = 0;
    int sign = 1;

    if (*num == '-') { sign = -1; num++; }
    while (*num >= '0' && *num <= '9') { n = n * 10 + (*num - '0'); num++; }
    if (*num == '.') {
        double frac = 0.1;
        num++;
        while (*num >= '0' && *num <= '9') { n += (*num - '0') * frac; frac *= 0.1; num++; }
    }
    if (*num == 'e' || *num == 'E') {
        int esign = 1, exp = 0;
        num++;
        if (*num == '-') { esign = -1; num++; }
        else if (*num == '+') num++;
        while (*num >= '0' && *num <= '9') { exp = exp * 10 + (*num - '0'); num++; }
        n *= pow(10.0, esign * exp);
    }
    n *= sign;

    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;
    return num;
}

static const char *parse_value(cJSON *item, const char *value);

static const char *parse_array(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '[') return NULL;
    item->type = cJSON_Array;
    value = skip_whitespace(value + 1);
    if (*value == ']') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!child) return NULL;
    value = skip_whitespace(parse_value(child, skip_whitespace(value)));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
    }

    if (*value == ']') return value + 1;
    return NULL;
}

static const char *parse_object(cJSON *item, const char *value) {
    cJSON *child;
    if (*value != '{') return NULL;
    item->type = cJSON_Object;
    value = skip_whitespace(value + 1);
    if (*value == '}') return value + 1;

    item->child = child = cJSON_New_Item();
    if (!child) return NULL;
    value = skip_whitespace(parse_string(child, skip_whitespace(value)));
    if (!value) return NULL;
    child->string = child->valuestring;
    child->valuestring = NULL;
    if (*value != ':') return NULL;
    value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
    if (!value) return NULL;

    while (*value == ',') {
        cJSON *new_item = cJSON_New_Item();
        if (!new_item) return NULL;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip_whitespace(parse_string(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
        child->string = child->valuestring;
        child->valuestring = NULL;
        if (*value != ':') return NULL;
        value = skip_whitespace(parse_value(child, skip_whitespace(value + 1)));
        if (!value) return NULL;
    }

    if (*value == '}') return value + 1;
    return NULL;
}

static const char *parse_value(cJSON *item, const char *value) {
    if (!value) return NULL;
    if (!strncmp(value, "null", 4))  { item->type = cJSON_NULL;  return value + 4; }
    if (!strncmp(value, "false", 5)) { item->type = cJSON_False; return value + 5; }
    if (!strncmp(value, "true", 4))  { item->type = cJSON_True;  return value + 4; }
    if (*value == '\"') return parse_string(item, value);
    if (*value == '-' || (*value >= '0' && *value <= '9')) return parse_number(item, value);
    if (*value == '[') return parse_array(item, value);
    if (*value == '{') return parse_object(item, value);
    return NULL;
}

cJSON *cJSON_Parse(const char *value) {
    cJSON *c = cJSON_New_Item();
    if (!c) return NULL;
    if (!parse_value(c, skip_whitespace(value))) {
        cJSON_Delete(c);
        return NULL;
    }
    return c;
}

/* ---- Printer ---- */
static char *print_string(const char *str) {
    int len = 2;
    const char *p = str;
    while (*p) {
        if (*p == '\"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') len++;
        len++;
        p++;
    }
    char *out = (char*)malloc(len + 1);
    if (!out) return NULL;
    char *o = out;
    *o++ = '\"';
    p = str;
    while (*p) {
        if (*p == '\"') { *o++ = '\\'; *o++ = '\"'; }
        else if (*p == '\\') { *o++ = '\\'; *o++ = '\\'; }
        else if (*p == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (*p == '\r') { *o++ = '\\'; *o++ = 'r'; }
        else if (*p == '\t') { *o++ = '\\'; *o++ = 't'; }
        else *o++ = *p;
        p++;
    }
    *o++ = '\"';
    *o = 0;
    return out;
}

static char *print_number(const cJSON *item) {
    char *out = (char*)malloc(64);
    if (!out) return NULL;
    if (item->valuedouble == (double)item->valueint) {
        snprintf(out, 64, "%d", item->valueint);
    } else {
        snprintf(out, 64, "%g", item->valuedouble);
    }
    return out;
}

static char *print_value(const cJSON *item, int depth, int fmt);

static char *print_array(const cJSON *item, int depth, int fmt) {
    char **entries;
    char *out, *ptr;
    int numentries = 0, len = 3;
    cJSON *child = item->child;

    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        out = (char*)malloc(3);
        if (out) strcpy(out, "[]");
        return out;
    }

    entries = (char**)calloc(numentries, sizeof(char*));
    if (!entries) return NULL;

    child = item->child;
    for (int i = 0; i < numentries; i++) {
        entries[i] = print_value(child, depth + 1, fmt);
        if (entries[i]) len += strlen(entries[i]) + 2 + (fmt ? depth + 1 : 0);
        child = child->next;
    }
    if (fmt) len += numentries * 2;

    out = (char*)malloc(len + 64);
    if (!out) { for (int i = 0; i < numentries; i++) free(entries[i]); free(entries); return NULL; }

    ptr = out;
    *ptr++ = '[';
    if (fmt) *ptr++ = '\n';
    for (int i = 0; i < numentries; i++) {
        if (fmt) for (int j = 0; j < depth + 1; j++) *ptr++ = '\t';
        if (entries[i]) { strcpy(ptr, entries[i]); ptr += strlen(entries[i]); free(entries[i]); }
        if (i < numentries - 1) *ptr++ = ',';
        if (fmt) *ptr++ = '\n';
    }
    if (fmt) for (int j = 0; j < depth; j++) *ptr++ = '\t';
    *ptr++ = ']';
    *ptr = 0;
    free(entries);
    return out;
}

static char *print_object(const cJSON *item, int depth, int fmt) {
    char **entries, **names;
    char *out, *ptr;
    int numentries = 0, len = 3;
    cJSON *child = item->child;

    while (child) { numentries++; child = child->next; }
    if (!numentries) {
        out = (char*)malloc(3);
        if (out) strcpy(out, "{}");
        return out;
    }

    entries = (char**)calloc(numentries, sizeof(char*));
    names = (char**)calloc(numentries, sizeof(char*));
    if (!entries || !names) { free(entries); free(names); return NULL; }

    child = item->child;
    for (int i = 0; i < numentries; i++) {
        names[i] = print_string(child->string);
        entries[i] = print_value(child, depth + 1, fmt);
        if (names[i]) len += strlen(names[i]) + (fmt ? 4 : 1) + (fmt ? depth + 1 : 0);
        if (entries[i]) len += strlen(entries[i]) + 1;
        child = child->next;
    }
    if (fmt) len += numentries * 2;

    out = (char*)malloc(len + 64);
    if (!out) {
        for (int i = 0; i < numentries; i++) { free(names[i]); free(entries[i]); }
        free(names); free(entries);
        return NULL;
    }

    ptr = out;
    *ptr++ = '{';
    if (fmt) *ptr++ = '\n';
    for (int i = 0; i < numentries; i++) {
        if (fmt) for (int j = 0; j < depth + 1; j++) *ptr++ = '\t';
        if (names[i]) { strcpy(ptr, names[i]); ptr += strlen(names[i]); free(names[i]); }
        *ptr++ = ':';
        if (fmt) *ptr++ = ' ';
        if (entries[i]) { strcpy(ptr, entries[i]); ptr += strlen(entries[i]); free(entries[i]); }
        if (i < numentries - 1) *ptr++ = ',';
        if (fmt) *ptr++ = '\n';
    }
    if (fmt) for (int j = 0; j < depth; j++) *ptr++ = '\t';
    *ptr++ = '}';
    *ptr = 0;
    free(names); free(entries);
    return out;
}

static char *print_value(const cJSON *item, int depth, int fmt) {
    if (!item) return NULL;
    switch (item->type) {
        case cJSON_NULL:   return cJSON_strdup("null");
        case cJSON_False:  return cJSON_strdup("false");
        case cJSON_True:   return cJSON_strdup("true");
        case cJSON_Number: return print_number(item);
        case cJSON_String: return print_string(item->valuestring);
        case cJSON_Array:  return print_array(item, depth, fmt);
        case cJSON_Object: return print_object(item, depth, fmt);
    }
    return NULL;
}

char *cJSON_Print(const cJSON *item) { return print_value(item, 0, 1); }
char *cJSON_PrintUnformatted(const cJSON *item) { return print_value(item, 0, 0); }

/* ---- Creation ---- */
cJSON *cJSON_CreateObject(void)           { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_Object; return i; }
cJSON *cJSON_CreateArray(void)            { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_Array; return i; }
cJSON *cJSON_CreateNull(void)             { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_NULL; return i; }
cJSON *cJSON_CreateTrue(void)             { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_True; return i; }
cJSON *cJSON_CreateFalse(void)            { cJSON *i = cJSON_New_Item(); if (i) i->type = cJSON_False; return i; }
cJSON *cJSON_CreateBool(int b)            { cJSON *i = cJSON_New_Item(); if (i) i->type = b ? cJSON_True : cJSON_False; return i; }
cJSON *cJSON_CreateNumber(double num)     { cJSON *i = cJSON_New_Item(); if (i) { i->type = cJSON_Number; i->valuedouble = num; i->valueint = (int)num; } return i; }
cJSON *cJSON_CreateString(const char *s)  { cJSON *i = cJSON_New_Item(); if (i) { i->type = cJSON_String; i->valuestring = cJSON_strdup(s ? s : ""); } return i; }

/* ---- Object/Array Operations ---- */
static void suffix_object(cJSON *prev, cJSON *item) {
    prev->next = item;
    item->prev = prev;
}

void cJSON_AddItemToArray(cJSON *array, cJSON *item) {
    if (!item || !array) return;
    cJSON *child = array->child;
    if (!child) { array->child = item; }
    else {
        while (child->next) child = child->next;
        suffix_object(child, item);
    }
}

void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item) {
    if (!item || !object) return;
    if (item->string) free(item->string);
    item->string = cJSON_strdup(string);
    cJSON_AddItemToArray(object, item);
}

cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string) {
    if (!object || !string) return NULL;
    cJSON *child = object->child;
    while (child && strcmp(child->string, string) != 0) child = child->next;
    return child;
}

void cJSON_DeleteItemFromObject(cJSON *object, const char *string) {
    if (!object || !string) return;
    cJSON *child = object->child;
    cJSON *prev_item = NULL;
    while (child) {
        if (child->string && strcmp(child->string, string) == 0) {
            if (prev_item) prev_item->next = child->next;
            else object->child = child->next;
            if (child->next) child->next->prev = prev_item;
            child->next = NULL;
            child->prev = NULL;
            cJSON_Delete(child);
            return;
        }
        prev_item = child;
        child = child->next;
    }
}

int cJSON_GetArraySize(const cJSON *array) {
    int size = 0;
    cJSON *child = array ? array->child : NULL;
    while (child) { size++; child = child->next; }
    return size;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index) {
    if (!array) return NULL;
    cJSON *child = array->child;
    while (child && index > 0) { child = child->next; index--; }
    return child;
}

/* ---- Utility ---- */
int cJSON_IsTrue(const cJSON *item)   { return item && item->type == cJSON_True; }
int cJSON_IsFalse(const cJSON *item)  { return item && item->type == cJSON_False; }
int cJSON_IsNull(const cJSON *item)   { return item && item->type == cJSON_NULL; }
int cJSON_IsNumber(const cJSON *item) { return item && item->type == cJSON_Number; }
int cJSON_IsString(const cJSON *item) { return item && item->type == cJSON_String; }
int cJSON_IsArray(const cJSON *item)  { return item && item->type == cJSON_Array; }
int cJSON_IsObject(const cJSON *item) { return item && item->type == cJSON_Object; }

cJSON *cJSON_Duplicate(const cJSON *item, int recurse) {
    if (!item) return NULL;
    cJSON *newitem = cJSON_New_Item();
    if (!newitem) return NULL;

    newitem->type = item->type;
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;
    if (item->valuestring) newitem->valuestring = cJSON_strdup(item->valuestring);
    if (item->string) newitem->string = cJSON_strdup(item->string);

    if (recurse && item->child) {
        cJSON *child = item->child;
        cJSON *newchild = cJSON_Duplicate(child, 1);
        newitem->child = newchild;
        child = child->next;
        while (child) {
            cJSON *nc = cJSON_Duplicate(child, 1);
            if (newchild) { newchild->next = nc; if (nc) nc->prev = newchild; }
            newchild = nc;
            child = child->next;
        }
    }
    return newitem;
}
