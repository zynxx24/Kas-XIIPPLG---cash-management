/*
 * XII PPLG Cash Management - HTTP Server Core
 */
#include "server.h"
#include "router.h"
#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static volatile int running = 1;

void url_decode(const char *src, char *dst, int dst_size) {
    int i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = { src[i+1], src[i+2], 0 };
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = 0;
}

void parse_http_request(const char *raw, int raw_len, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));
    req->body = NULL;

    /* Parse method */
    const char *p = raw;
    int i = 0;
    while (*p && *p != ' ' && i < 15) req->method[i++] = *p++;
    req->method[i] = 0;

    /* Skip space */
    while (*p == ' ') p++;

    /* Parse path and query */
    i = 0;
    while (*p && *p != ' ' && *p != '?' && i < MAX_PATH_LEN - 1) req->path[i++] = *p++;
    req->path[i] = 0;

    if (*p == '?') {
        p++;
        i = 0;
        while (*p && *p != ' ' && i < MAX_PATH_LEN - 1) req->query[i++] = *p++;
        req->query[i] = 0;
    }

    /* Parse headers to find content-type, content-length, auth */
    const char *headers = strstr(raw, "\r\n");
    if (headers) headers += 2;

    while (headers && *headers && !(headers[0] == '\r' && headers[1] == '\n')) {
        if (strncasecmp(headers, "Content-Type:", 13) == 0) {
            const char *val = headers + 13;
            while (*val == ' ') val++;
            i = 0;
            while (*val && *val != '\r' && i < 255) req->content_type[i++] = *val++;
            req->content_type[i] = 0;
        }
        else if (strncasecmp(headers, "Content-Length:", 15) == 0) {
            req->content_length = atoi(headers + 15);
        }
        else if (strncasecmp(headers, "Authorization:", 14) == 0) {
            const char *val = headers + 14;
            while (*val == ' ') val++;
            if (strncasecmp(val, "Bearer ", 7) == 0) val += 7;
            i = 0;
            while (*val && *val != '\r' && i < 511) req->auth_token[i++] = *val++;
            req->auth_token[i] = 0;
        }
        headers = strstr(headers, "\r\n");
        if (headers) headers += 2;
    }

    /* Parse body */
    const char *body = strstr(raw, "\r\n\r\n");
    if (body) {
        body += 4;
        int body_len = raw_len - (body - raw);
        if (body_len > MAX_BODY_LEN - 1) body_len = MAX_BODY_LEN - 1;
        if (body_len > 0) {
            req->body = (char*)malloc(body_len + 1);
            if (req->body) {
                memcpy(req->body, body, body_len);
                req->body[body_len] = 0;
            }
        }
    }
    /* Ensure body is never NULL for consumer code */
    if (!req->body) {
        req->body = (char*)calloc(1, 1);
    }
}

void send_http_response(int client_fd, HttpResponse *res) {
    char header[2048];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        res->status, res->status_text,
        res->content_type[0] ? res->content_type : "text/plain",
        res->body_length,
        res->extra_headers);

    send(client_fd, header, hlen, 0);
    if (res->body && res->body_length > 0) {
        int sent = 0;
        while (sent < res->body_length) {
            int n = send(client_fd, res->body + sent, res->body_length - sent, 0);
            if (n <= 0) break;
            sent += n;
        }
    }
}

void response_json(HttpResponse *res, int status, const char *json) {
    res->status = status;
    snprintf(res->status_text, 64, "%s",
        status == 200 ? "OK" :
        status == 201 ? "Created" :
        status == 400 ? "Bad Request" :
        status == 401 ? "Unauthorized" :
        status == 403 ? "Forbidden" :
        status == 404 ? "Not Found" :
        status == 429 ? "Too Many Requests" :
        "Internal Server Error");
    strncpy(res->content_type, "application/json", 255);
    res->body = strdup(json);
    res->body_length = strlen(json);
}

void response_error(HttpResponse *res, int status, const char *message) {
    char json[1024];
    snprintf(json, sizeof(json), "{\"error\":\"%s\"}", message);
    response_json(res, status, json);
}

void response_file(HttpResponse *res, const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        response_error(res, 404, "File not found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > MAX_RESPONSE_SIZE) {
        fclose(f);
        response_error(res, 500, "File too large");
        return;
    }

    char *buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        response_error(res, 500, "Memory error");
        return;
    }

    fread(buf, 1, size, f);
    fclose(f);
    buf[size] = 0;

    res->status = 200;
    strcpy(res->status_text, "OK");
    strncpy(res->content_type, get_mime_type(filepath), 255);
    res->body = buf;
    res->body_length = (int)size;
}

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} ClientArgs;

static void *handle_client(void *arg) {
    ClientArgs *ca = (ClientArgs*)arg;
    int client_fd = ca->client_fd;
    char client_ip[64];
    inet_ntop(AF_INET, &ca->client_addr.sin_addr, client_ip, sizeof(client_ip));
    free(ca);

    /* Security check */
    if (!security_check_ip(client_ip)) {
        const char *resp = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(client_fd, resp, strlen(resp), 0);
        close(client_fd);
        return NULL;
    }
    security_record_request(client_ip);

    /* Read request */
    char *buffer = (char*)malloc(MAX_REQUEST_SIZE);
    if (!buffer) {
        close(client_fd);
        return NULL;
    }
    memset(buffer, 0, MAX_REQUEST_SIZE);
    int total = 0;
    int n;

    /* Read headers first */
    while (total < MAX_REQUEST_SIZE - 1) {
        n = recv(client_fd, buffer + total, MAX_REQUEST_SIZE - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        buffer[total] = 0;
        if (strstr(buffer, "\r\n\r\n")) break;
    }

    /* If there's a content-length, read the rest of the body */
    if (total > 0) {
        char *cl = strcasestr(buffer, "Content-Length:");
        if (cl) {
            int content_length = atoi(cl + 15);
            char *body_start = strstr(buffer, "\r\n\r\n");
            if (body_start) {
                int header_len = (body_start + 4) - buffer;
                int body_received = total - header_len;
                while (body_received < content_length && total < MAX_REQUEST_SIZE - 1) {
                    n = recv(client_fd, buffer + total, MAX_REQUEST_SIZE - 1 - total, 0);
                    if (n <= 0) break;
                    total += n;
                    body_received += n;
                }
            }
        }
    }

    if (total <= 0) {
        free(buffer);
        close(client_fd);
        return NULL;
    }

    /* Parse and route */
    HttpRequest req;
    parse_http_request(buffer, total, &req);
    free(buffer);
    strncpy(req.client_ip, client_ip, 63);

    HttpResponse res;
    memset(&res, 0, sizeof(res));

    /* Handle CORS preflight */
    if (strcmp(req.method, "OPTIONS") == 0) {
        res.status = 204;
        strcpy(res.status_text, "No Content");
        strcpy(res.content_type, "text/plain");
        res.body = strdup("");
        res.body_length = 0;
    } else {
        router_handle(&req, &res);
    }

    send_http_response(client_fd, &res);

    if (req.body) free(req.body);
    if (res.body) free(res.body);
    close(client_fd);
    return NULL;
}

int server_init(int port) {
    int server_fd;
    struct sockaddr_in addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[ERROR] Socket creation failed");
        return -1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ERROR] Bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("[ERROR] Listen failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

void server_run(int server_fd) {
    printf("[SERVER] Listening on port %d...\n", SERVER_PORT);
    printf("[SERVER] Open http://localhost:%d in your browser\n\n", SERVER_PORT);

    while (running) {
        if (security_should_shutdown()) {
            printf("[SECURITY] Attack detected! CPU threshold exceeded. Shutting down...\n");
            break;
        }

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        /* Set accept timeout so we can check shutdown flag */
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity <= 0) continue;

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) continue;

        /* Set socket timeout */
        struct timeval client_tv = { .tv_sec = 30, .tv_usec = 0 };
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &client_tv, sizeof(client_tv));

        ClientArgs *args = (ClientArgs*)malloc(sizeof(ClientArgs));
        args->client_fd = client_fd;
        args->client_addr = client_addr;

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&thread, &attr, handle_client, args) != 0) {
            close(client_fd);
            free(args);
        }
        pthread_attr_destroy(&attr);
    }

    close(server_fd);
}

void server_shutdown(void) {
    running = 0;
}
