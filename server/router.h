/*
 * XII PPLG Cash Management - Request Router
 */
#ifndef ROUTER_H
#define ROUTER_H

#include "server.h"

/* Initialize router */
void router_init(void);

/* Route a request and fill response */
void router_handle(HttpRequest *req, HttpResponse *res);

/* Get MIME type from file extension */
const char *get_mime_type(const char *path);

#endif /* ROUTER_H */
