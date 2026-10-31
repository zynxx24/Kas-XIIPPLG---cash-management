/*
 * XII PPLG Cash Management - Security Module
 * Rate limiting, CPU monitoring, attack detection
 */
#ifndef SECURITY_H
#define SECURITY_H

#define MAX_TRACKED_IPS    1024
#define RATE_LIMIT_WINDOW  10    /* seconds */
#define RATE_LIMIT_MAX     100   /* max requests per window */
#define CPU_CHECK_INTERVAL 5     /* seconds */
#define CPU_THRESHOLD      90.0  /* percent - trigger shutdown */
#define BAN_DURATION       300   /* seconds */

typedef struct {
    char ip[64];
    int  request_count;
    long window_start;
    long banned_until;
} IpTracker;

/* Initialize security module */
void security_init(void);

/* Check if IP is allowed (returns 1 if allowed, 0 if blocked) */
int security_check_ip(const char *ip);

/* Record a request from IP */
void security_record_request(const char *ip);

/* Get current CPU usage (0-100) */
double security_get_cpu_usage(void);

/* Start CPU monitoring thread */
void security_start_monitor(void);

/* Check if server should shut down due to attack */
int security_should_shutdown(void);

/* Get security stats as JSON */
char *security_get_stats(void);

#endif /* SECURITY_H */
