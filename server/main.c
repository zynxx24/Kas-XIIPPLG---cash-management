/*
 * XII PPLG Cash Management Server
 * Secure C HTTP Server with Rate Limiting & CPU Monitoring
 *
 * Build: make
 * Run:   ./server
 */
#include "server.h"
#include "router.h"
#include "database.h"
#include "auth.h"
#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static int server_fd = -1;

static void signal_handler(int sig) {
    printf("\n[SERVER] Received signal %d, shutting down...\n", sig);
    server_shutdown();
    if (server_fd >= 0) close(server_fd);
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    /* Banner */
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     XII PPLG CASH MANAGEMENT SERVER             ║\n");
    printf("║     Secure C Backend v1.0                       ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  Features:                                      ║\n");
    printf("║  ✓ Multi-threaded HTTP Server                   ║\n");
    printf("║  ✓ JSON File-based Database                     ║\n");
    printf("║  ✓ Token Authentication                         ║\n");
    printf("║  ✓ Per-IP Rate Limiting                         ║\n");
    printf("║  ✓ CPU Monitoring & Auto-Shutdown               ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    /* Signal handling */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Initialize modules */
    db_init();
    auth_init();
    db_start_backup_timer();
    security_init();
    router_init();

    /* Start CPU monitor */
    security_start_monitor();

    /* Start server */
    server_fd = server_init(port);
    if (server_fd < 0) {
        fprintf(stderr, "[ERROR] Failed to start server on port %d\n", port);
        return 1;
    }

    server_run(server_fd);

    printf("[SERVER] Server stopped.\n");
    return 0;
}
