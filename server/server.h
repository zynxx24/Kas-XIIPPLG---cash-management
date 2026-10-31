/*
 * XII PPLG Cash Management - HTTP Server Core
 */
#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

#define MAX_REQUEST_SIZE  20971520
#define MAX_RESPONSE_SIZE 25165824
#define MAX_PATH_LEN      1024
#define MAX_HEADER_LEN    4096
#define MAX_BODY_LEN      20971520
#define SERVER_PORT        8080
#define MAX_CONNECTIONS    100
#define THREAD_POOL_SIZE   16

typedef struct {
    char method[16];
    char path[MAX_PATH_LEN];
    char query[MAX_PATH_LEN];
    char *body;
    char content_type[256];
    char auth_token[512];
    char client_ip[64];
    int  content_length;
} HttpRequest;

typedef struct {
    int   status;
    char  status_text[64];
    char  content_type[256];
    char *body;
    int   body_length;
    char  extra_headers[1024];
} HttpResponse;

/* Server lifecycle */
int  server_init(int port);
void server_run(int server_fd);
void server_shutdown(void);

/* HTTP helpers */
void parse_http_request(const char *raw, int raw_len, HttpRequest *req);
void send_http_response(int client_fd, HttpResponse *res);
void response_json(HttpResponse *res, int status, const char *json);
void response_error(HttpResponse *res, int status, const char *message);
void response_file(HttpResponse *res, const char *filepath);

/* URL decode */
void url_decode(const char *src, char *dst, int dst_size);

#endif /* SERVER_H */
