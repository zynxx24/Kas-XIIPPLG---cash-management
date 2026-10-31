/*
 * XII PPLG Cash Management - Request Router
 */
#include "router.h"
#include "api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PUBLIC_DIR "../public"

const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    ext++;
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, "css") == 0)  return "text/css; charset=utf-8";
    if (strcasecmp(ext, "js") == 0)   return "application/javascript; charset=utf-8";
    if (strcasecmp(ext, "json") == 0) return "application/json";
    if (strcasecmp(ext, "png") == 0)  return "image/png";
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, "gif") == 0)  return "image/gif";
    if (strcasecmp(ext, "svg") == 0)  return "image/svg+xml";
    if (strcasecmp(ext, "webp") == 0) return "image/webp";
    if (strcasecmp(ext, "ico") == 0)  return "image/x-icon";
    if (strcasecmp(ext, "woff2") == 0) return "font/woff2";
    if (strcasecmp(ext, "woff") == 0) return "font/woff";
    if (strcasecmp(ext, "ttf") == 0)  return "font/ttf";
    if (strcasecmp(ext, "pdf") == 0)  return "application/pdf";
    return "application/octet-stream";
}

void router_init(void) {
    printf("[ROUTER] Router initialized\n");
}
static void serve_static(HttpRequest *req, HttpResponse *res) {
    char filepath[2048];
    const char *path = req->path;

    /* Default to dashboard.html for root */
    if (strcmp(path, "/") == 0) {
        path = "/dashboard.html";
    }

    /* Prevent directory traversal */
    if (strstr(path, "..") != NULL) {
        response_error(res, 403, "Forbidden");
        return;
    }

    /* Treat /asset/ routes differently (served from root's asset folder) */
    if (strncmp(path, "/asset/", 7) == 0) {
        snprintf(filepath, sizeof(filepath), "..%s", path);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", PUBLIC_DIR, path);
    }

    /* Check if file exists */
    struct stat st;
    if (stat(filepath, &st) != 0 || S_ISDIR(st.st_mode)) {
        /* Try appending .html */
        char htmlpath[2048];
        snprintf(htmlpath, sizeof(htmlpath), "%s.html", filepath);
        if (stat(htmlpath, &st) == 0) {
            response_file(res, htmlpath);
            return;
        }
        response_error(res, 404, "Not Found");
        return;
    }

    response_file(res, filepath);
}

void router_handle(HttpRequest *req, HttpResponse *res) {
    const char *path = req->path;
    const char *method = req->method;

    /* API Routes */
    if (strncmp(path, "/api/", 5) == 0) {
        /* Auth endpoints (no token required) */
        if (strcmp(path, "/api/login") == 0 && strcmp(method, "POST") == 0) {
            api_login(req, res);
            return;
        }
        if (strcmp(path, "/api/logout") == 0 && strcmp(method, "POST") == 0) {
            api_logout(req, res);
            return;
        }

        /* All other API routes - organized by resource */
        if (strcmp(path, "/api/members") == 0 && strcmp(method, "GET") == 0) {
            api_get_members(req, res);
        }
        else if (strcmp(path, "/api/dashboard") == 0 && strcmp(method, "GET") == 0) {
            api_get_dashboard(req, res);
        }
        else if (strcmp(path, "/api/payments") == 0 && strcmp(method, "GET") == 0) {
            api_get_payments(req, res);
        }
        else if (strcmp(path, "/api/payments") == 0 && strcmp(method, "POST") == 0) {
            api_save_payments(req, res);
        }
        else if (strcmp(path, "/api/donations") == 0 && strcmp(method, "GET") == 0) {
            api_get_donations(req, res);
        }
        else if (strcmp(path, "/api/donations") == 0 && strcmp(method, "POST") == 0) {
            api_add_donation(req, res);
        }
        else if (strncmp(path, "/api/donations/", 15) == 0 && strcmp(method, "DELETE") == 0) {
            api_delete_donation(req, res);
        }
        else if (strcmp(path, "/api/expenses") == 0 && strcmp(method, "GET") == 0) {
            api_get_expenses(req, res);
        }
        else if (strcmp(path, "/api/expenses") == 0 && strcmp(method, "POST") == 0) {
            api_add_expense(req, res);
        }
        else if (strncmp(path, "/api/expenses/", 14) == 0 && strcmp(method, "DELETE") == 0) {
            api_delete_expense(req, res);
        }
        else if (strcmp(path, "/api/config") == 0 && strcmp(method, "GET") == 0) {
            api_get_config(req, res);
        }
        else if (strcmp(path, "/api/config") == 0 && strcmp(method, "POST") == 0) {
            api_save_config(req, res);
        }
        else if (strcmp(path, "/api/security") == 0 && strcmp(method, "GET") == 0) {
            api_get_security(req, res);
        }
        else {
            response_error(res, 404, "API endpoint not found");
        }
        return;
    }

    /* Static files */
    serve_static(req, res);
}
