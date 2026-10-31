/*
 * XII PPLG Cash Management - API Handlers
 */
#include "api.h"
#include "auth.h"
#include "database.h"
#include "security.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Session *require_auth(HttpRequest *req, HttpResponse *res) {
    /* If there's a valid token, use it */
    if (strlen(req->auth_token) > 0) {
        Session *session = auth_validate(req->auth_token);
        if (session) return session;
    }
    /* No token or invalid/expired token -> guest mode (no 401) */
    (void)res;
    static Session guest_session;
    strcpy(guest_session.token, "guest");
    strcpy(guest_session.username, "siswa");
    strcpy(guest_session.role, "user");
    guest_session.member_id = 0;
    guest_session.expires = 0;
    return &guest_session;
}

static Session *require_admin(HttpRequest *req, HttpResponse *res) {
    Session *session = require_auth(req, res);
    if (session && strcmp(session->role, "admin") != 0) {
        response_error(res, 403, "Admin access required");
        return NULL;
    }
    return session;
}

/* ---- Login / Logout ---- */
void api_login(HttpRequest *req, HttpResponse *res) {
    cJSON *body = cJSON_Parse(req->body);
    if (!body) {
        response_error(res, 400, "Invalid JSON");
        return;
    }

    cJSON *role = cJSON_GetObjectItem(body, "role");
    if (!role || !role->valuestring) {
        cJSON_Delete(body);
        response_error(res, 400, "Role required");
        return;
    }

    char role_str[32] = "";
    strncpy(role_str, role->valuestring, sizeof(role_str) - 1);

    char *token = NULL;

    if (strcmp(role_str, "admin") == 0) {
        cJSON *username = cJSON_GetObjectItem(body, "username");
        cJSON *password = cJSON_GetObjectItem(body, "password");
        if (!username || !password || !username->valuestring || !password->valuestring) {
            cJSON_Delete(body);
            response_error(res, 400, "Username and password required");
            return;
        }
        token = auth_login(username->valuestring, password->valuestring, "admin", 0);
    }
    else if (strcmp(role_str, "user") == 0) {
        cJSON *member_id = cJSON_GetObjectItem(body, "member_id");
        cJSON *name = cJSON_GetObjectItem(body, "name");
        if (!member_id) {
            cJSON_Delete(body);
            response_error(res, 400, "Member ID required");
            return;
        }
        const char *uname = name && name->valuestring ? name->valuestring : "student";
        token = auth_login(uname, "", "user", member_id->valueint);
    }

    cJSON_Delete(body);

    if (!token) {
        response_error(res, 401, "Invalid credentials");
        return;
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "token", token);
    cJSON_AddStringToObject(resp, "role", role_str);
    char *json = cJSON_PrintUnformatted(resp);
    response_json(res, 200, json);
    cJSON_Delete(resp);
    free(json);
    free(token);
}

void api_logout(HttpRequest *req, HttpResponse *res) {
    auth_logout(req->auth_token);
    response_json(res, 200, "{\"message\":\"Logged out\"}");
}

/* ---- Members ---- */
void api_get_members(HttpRequest *req, HttpResponse *res) {
    if (!require_auth(req, res)) return;

    cJSON *members = db_get_members();
    if (!members) {
        response_error(res, 500, "Failed to read members");
        return;
    }

    char *json = cJSON_PrintUnformatted(members);
    response_json(res, 200, json);
    cJSON_Delete(members);
    free(json);
}

/* ---- Dashboard ---- */
void api_get_dashboard(HttpRequest *req, HttpResponse *res) {
    if (!require_auth(req, res)) return;

    cJSON *config = db_get_config();
    cJSON *payments = db_get_payments();
    cJSON *donations = db_get_donations();
    cJSON *expenses = db_get_expenses();
    cJSON *members = db_get_members();

    int kas_amount = 5000;
    int previous_balance = 0;
    if (config) {
        cJSON *ka = cJSON_GetObjectItem(config, "kas_amount");
        cJSON *pb = cJSON_GetObjectItem(config, "previous_balance");
        if (ka) kas_amount = ka->valueint;
        if (pb) previous_balance = pb->valueint;
    }

    /* Count total payments */
    int total_payments = 0;
    int total_payment_dates = 0;
    int total_members = members ? cJSON_GetArraySize(members) : 33;

    if (payments) {
        cJSON *dates = cJSON_GetObjectItem(payments, "dates");
        cJSON *records = cJSON_GetObjectItem(payments, "records");
        total_payment_dates = dates ? cJSON_GetArraySize(dates) : 0;

        if (records) {
            cJSON *member_rec = records->child;
            while (member_rec) {
                cJSON *entry = member_rec->child;
                while (entry) {
                    if (cJSON_IsTrue(entry)) total_payments++;
                    entry = entry->next;
                }
                member_rec = member_rec->next;
            }
        }
    }

    int total_kas_income = total_payments * kas_amount;

    /* Count donations */
    int total_donation_amount = 0;
    int total_donation_count = 0;
    if (donations) {
        total_donation_count = cJSON_GetArraySize(donations);
        for (int i = 0; i < total_donation_count; i++) {
            cJSON *d = cJSON_GetArrayItem(donations, i);
            cJSON *amt = cJSON_GetObjectItem(d, "amount");
            if (amt) total_donation_amount += amt->valueint;
        }
    }

    /* Count expenses */
    int total_expense_amount = 0;
    int total_expense_count = 0;
    if (expenses) {
        total_expense_count = cJSON_GetArraySize(expenses);
        for (int i = 0; i < total_expense_count; i++) {
            cJSON *e = cJSON_GetArrayItem(expenses, i);
            cJSON *amt = cJSON_GetObjectItem(e, "amount");
            if (amt) total_expense_amount += amt->valueint;
        }
    }

    int total_balance = previous_balance + total_kas_income + total_donation_amount - total_expense_amount;

    /* Get server's current date */
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char server_date[16];
    strftime(server_date, sizeof(server_date), "%Y-%m-%d", tm_now);

    /* Payment progress for current/latest period (latest date <= today) */
    int latest_paid = 0;
    if (payments && total_payment_dates > 0) {
        cJSON *dates = cJSON_GetObjectItem(payments, "dates");
        cJSON *records = cJSON_GetObjectItem(payments, "records");

        /* Find the latest date that is <= server_date */
        cJSON *latest_date = NULL;
        for (int i = total_payment_dates - 1; i >= 0; i--) {
            cJSON *d = cJSON_GetArrayItem(dates, i);
            if (d && d->valuestring && strcmp(d->valuestring, server_date) <= 0) {
                latest_date = d;
                break;
            }
        }

        if (latest_date && records) {
            cJSON *member_rec = records->child;
            while (member_rec) {
                cJSON *entry = cJSON_GetObjectItem(member_rec, latest_date->valuestring);
                if (entry && cJSON_IsTrue(entry)) latest_paid++;
                member_rec = member_rec->next;
            }
        }
    }

    cJSON *dash = cJSON_CreateObject();
    cJSON_AddNumberToObject(dash, "total_balance", total_balance);
    cJSON_AddNumberToObject(dash, "previous_balance", previous_balance);
    cJSON_AddNumberToObject(dash, "kas_amount", kas_amount);
    cJSON_AddNumberToObject(dash, "total_kas_income", total_kas_income);
    cJSON_AddNumberToObject(dash, "total_payments", total_payments);
    cJSON_AddNumberToObject(dash, "total_payment_dates", total_payment_dates);
    cJSON_AddNumberToObject(dash, "total_members", total_members);
    cJSON_AddNumberToObject(dash, "total_donation_amount", total_donation_amount);
    cJSON_AddNumberToObject(dash, "total_donation_count", total_donation_count);
    cJSON_AddNumberToObject(dash, "total_expense_amount", total_expense_amount);
    cJSON_AddNumberToObject(dash, "total_expense_count", total_expense_count);
    cJSON_AddNumberToObject(dash, "latest_paid", latest_paid);
    cJSON_AddStringToObject(dash, "server_date", server_date);

    char *json = cJSON_PrintUnformatted(dash);
    response_json(res, 200, json);

    cJSON_Delete(dash);
    if (config) cJSON_Delete(config);
    if (payments) cJSON_Delete(payments);
    if (donations) cJSON_Delete(donations);
    if (expenses) cJSON_Delete(expenses);
    if (members) cJSON_Delete(members);
    free(json);
}

/* ---- Payments ---- */
void api_get_payments(HttpRequest *req, HttpResponse *res) {
    if (!require_auth(req, res)) return;

    cJSON *payments = db_get_payments();
    if (!payments) {
        response_error(res, 500, "Failed to read payments");
        return;
    }

    char *json = cJSON_PrintUnformatted(payments);
    response_json(res, 200, json);
    cJSON_Delete(payments);
    free(json);
}

void api_save_payments(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    cJSON *body = cJSON_Parse(req->body);
    if (!body) {
        response_error(res, 400, "Invalid JSON");
        return;
    }

    if (db_save_payments(body) != 0) {
        cJSON_Delete(body);
        response_error(res, 500, "Failed to save payments");
        return;
    }

    cJSON_Delete(body);
    response_json(res, 200, "{\"message\":\"Payments saved\"}");
}

/* ---- Donations ---- */
void api_get_donations(HttpRequest *req, HttpResponse *res) {
    if (!require_auth(req, res)) return;

    cJSON *donations = db_get_donations();
    if (!donations) {
        response_error(res, 500, "Failed to read donations");
        return;
    }

    char *json = cJSON_PrintUnformatted(donations);
    response_json(res, 200, json);
    cJSON_Delete(donations);
    free(json);
}

void api_add_donation(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    cJSON *body = cJSON_Parse(req->body);
    if (!body) {
        response_error(res, 400, "Invalid JSON");
        return;
    }

    cJSON *donations = db_get_donations();
    if (!donations) donations = cJSON_CreateArray();

    /* Generate ID */
    int max_id = 0;
    int size = cJSON_GetArraySize(donations);
    for (int i = 0; i < size; i++) {
        cJSON *d = cJSON_GetArrayItem(donations, i);
        cJSON *id = cJSON_GetObjectItem(d, "id");
        if (id && id->valueint > max_id) max_id = id->valueint;
    }

    cJSON_AddNumberToObject(body, "id", max_id + 1);
    cJSON_AddItemToArray(donations, cJSON_Duplicate(body, 1));

    if (db_save_donations(donations) != 0) {
        cJSON_Delete(body);
        cJSON_Delete(donations);
        response_error(res, 500, "Failed to save donation");
        return;
    }

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "{\"id\":%d}", max_id + 1);
    response_json(res, 201, id_str);
    cJSON_Delete(body);
    cJSON_Delete(donations);
}

void api_delete_donation(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    /* Extract ID from path: /api/donations/123 */
    int id = atoi(req->path + 15);
    if (id <= 0) {
        response_error(res, 400, "Invalid donation ID");
        return;
    }

    cJSON *donations = db_get_donations();
    if (!donations) {
        response_error(res, 404, "No donations found");
        return;
    }

    cJSON *new_arr = cJSON_CreateArray();
    int found = 0;
    int size = cJSON_GetArraySize(donations);
    for (int i = 0; i < size; i++) {
        cJSON *d = cJSON_GetArrayItem(donations, i);
        cJSON *did = cJSON_GetObjectItem(d, "id");
        if (did && did->valueint == id) {
            found = 1;
        } else {
            cJSON_AddItemToArray(new_arr, cJSON_Duplicate(d, 1));
        }
    }

    if (!found) {
        cJSON_Delete(new_arr);
        cJSON_Delete(donations);
        response_error(res, 404, "Donation not found");
        return;
    }

    db_save_donations(new_arr);
    cJSON_Delete(new_arr);
    cJSON_Delete(donations);
    response_json(res, 200, "{\"message\":\"Donation deleted\"}");
}

/* ---- Config ---- */
void api_get_config(HttpRequest *req, HttpResponse *res) {
    if (!require_auth(req, res)) return;

    cJSON *config = db_get_config();
    if (!config) {
        response_error(res, 500, "Failed to read config");
        return;
    }

    char *json = cJSON_PrintUnformatted(config);
    response_json(res, 200, json);
    cJSON_Delete(config);
    free(json);
}

void api_save_config(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    cJSON *body = cJSON_Parse(req->body);
    if (!body) {
        response_error(res, 400, "Invalid JSON");
        return;
    }

    /* Merge with existing config */
    cJSON *config = db_get_config();
    if (!config) config = cJSON_CreateObject();

    cJSON *item = body->child;
    while (item) {
        cJSON_DeleteItemFromObject(config, item->string);
        cJSON_AddItemToObject(config, item->string, cJSON_Duplicate(item, 1));
        item = item->next;
    }

    if (db_save_config(config) != 0) {
        cJSON_Delete(body);
        cJSON_Delete(config);
        response_error(res, 500, "Failed to save config");
        return;
    }

    cJSON_Delete(body);
    cJSON_Delete(config);
    response_json(res, 200, "{\"message\":\"Config saved\"}");
}

/* ---- Security ---- */
void api_get_security(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    char *stats = security_get_stats();
    response_json(res, 200, stats);
    free(stats);
}

/* ---- Expenses ---- */
void api_get_expenses(HttpRequest *req, HttpResponse *res) {
    if (!require_auth(req, res)) return;

    cJSON *expenses = db_get_expenses();
    if (!expenses) {
        response_error(res, 500, "Failed to read expenses");
        return;
    }

    char *json = cJSON_PrintUnformatted(expenses);
    response_json(res, 200, json);
    cJSON_Delete(expenses);
    free(json);
}

void api_add_expense(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    cJSON *body = cJSON_Parse(req->body);
    if (!body) {
        response_error(res, 400, "Invalid JSON");
        return;
    }

    cJSON *expenses = db_get_expenses();
    if (!expenses) expenses = cJSON_CreateArray();

    /* Generate ID */
    int max_id = 0;
    int size = cJSON_GetArraySize(expenses);
    for (int i = 0; i < size; i++) {
        cJSON *e = cJSON_GetArrayItem(expenses, i);
        cJSON *id = cJSON_GetObjectItem(e, "id");
        if (id && id->valueint > max_id) max_id = id->valueint;
    }

    cJSON_AddNumberToObject(body, "id", max_id + 1);
    cJSON_AddItemToArray(expenses, cJSON_Duplicate(body, 1));

    if (db_save_expenses(expenses) != 0) {
        cJSON_Delete(body);
        cJSON_Delete(expenses);
        response_error(res, 500, "Failed to save expense");
        return;
    }

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "{\"id\":%d}", max_id + 1);
    response_json(res, 201, id_str);
    cJSON_Delete(body);
    cJSON_Delete(expenses);
}

void api_delete_expense(HttpRequest *req, HttpResponse *res) {
    if (!require_admin(req, res)) return;

    /* Extract ID from path: /api/expenses/123 */
    int id = atoi(req->path + 14);
    if (id <= 0) {
        response_error(res, 400, "Invalid expense ID");
        return;
    }

    cJSON *expenses = db_get_expenses();
    if (!expenses) {
        response_error(res, 404, "No expenses found");
        return;
    }

    cJSON *new_arr = cJSON_CreateArray();
    int found = 0;
    int size = cJSON_GetArraySize(expenses);
    for (int i = 0; i < size; i++) {
        cJSON *e = cJSON_GetArrayItem(expenses, i);
        cJSON *eid = cJSON_GetObjectItem(e, "id");
        if (eid && eid->valueint == id) {
            found = 1;
        } else {
            cJSON_AddItemToArray(new_arr, cJSON_Duplicate(e, 1));
        }
    }

    if (!found) {
        cJSON_Delete(new_arr);
        cJSON_Delete(expenses);
        response_error(res, 404, "Expense not found");
        return;
    }

    db_save_expenses(new_arr);
    cJSON_Delete(new_arr);
    cJSON_Delete(expenses);
    response_json(res, 200, "{\"message\":\"Expense deleted\"}");
}
