/*
 * XII PPLG Cash Management - API Handlers
 */
#ifndef API_H
#define API_H

#include "server.h"

/* API endpoint handlers */
void api_login(HttpRequest *req, HttpResponse *res);
void api_logout(HttpRequest *req, HttpResponse *res);
void api_get_members(HttpRequest *req, HttpResponse *res);
void api_get_dashboard(HttpRequest *req, HttpResponse *res);

/* Payments */
void api_get_payments(HttpRequest *req, HttpResponse *res);
void api_save_payments(HttpRequest *req, HttpResponse *res);

/* Donations */
void api_get_donations(HttpRequest *req, HttpResponse *res);
void api_add_donation(HttpRequest *req, HttpResponse *res);
void api_delete_donation(HttpRequest *req, HttpResponse *res);


/* Expenses */
void api_get_expenses(HttpRequest *req, HttpResponse *res);
void api_add_expense(HttpRequest *req, HttpResponse *res);
void api_delete_expense(HttpRequest *req, HttpResponse *res);

/* Config */
void api_get_config(HttpRequest *req, HttpResponse *res);
void api_save_config(HttpRequest *req, HttpResponse *res);

/* Security */
void api_get_security(HttpRequest *req, HttpResponse *res);


#endif /* API_H */
