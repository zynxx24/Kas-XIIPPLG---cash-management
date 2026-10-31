/*
 * XII PPLG Cash Management - Security Module
 */
#include "security.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

static IpTracker ip_trackers[MAX_TRACKED_IPS];
static int num_tracked = 0;
static pthread_mutex_t sec_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int should_shutdown = 0;
static double current_cpu = 0.0;
static long total_requests = 0;
static int blocked_requests = 0;

void security_init(void) {
    memset(ip_trackers, 0, sizeof(ip_trackers));
    num_tracked = 0;
    should_shutdown = 0;
    current_cpu = 0.0;
    total_requests = 0;
    blocked_requests = 0;
    printf("[SECURITY] Security module initialized\n");
    printf("[SECURITY] Rate limit: %d req/%ds per IP\n", RATE_LIMIT_MAX, RATE_LIMIT_WINDOW);
    printf("[SECURITY] CPU threshold: %.0f%%\n", CPU_THRESHOLD);
}

static IpTracker *find_tracker(const char *ip) {
    for (int i = 0; i < num_tracked; i++) {
        if (strcmp(ip_trackers[i].ip, ip) == 0) {
            return &ip_trackers[i];
        }
    }
    return NULL;
}

static IpTracker *create_tracker(const char *ip) {
    if (num_tracked >= MAX_TRACKED_IPS) {
        /* Evict oldest entry */
        long oldest = ip_trackers[0].window_start;
        int idx = 0;
        for (int i = 1; i < num_tracked; i++) {
            if (ip_trackers[i].window_start < oldest) {
                oldest = ip_trackers[i].window_start;
                idx = i;
            }
        }
        ip_trackers[idx] = ip_trackers[num_tracked - 1];
        num_tracked--;
    }

    IpTracker *t = &ip_trackers[num_tracked];
    memset(t, 0, sizeof(IpTracker));
    strncpy(t->ip, ip, 63);
    t->window_start = time(NULL);
    num_tracked++;
    return t;
}

int security_check_ip(const char *ip) {
    pthread_mutex_lock(&sec_mutex);
    IpTracker *t = find_tracker(ip);

    if (t && t->banned_until > time(NULL)) {
        blocked_requests++;
        pthread_mutex_unlock(&sec_mutex);
        printf("[SECURITY] Blocked request from banned IP: %s\n", ip);
        return 0;
    }

    pthread_mutex_unlock(&sec_mutex);
    return 1;
}

void security_record_request(const char *ip) {
    pthread_mutex_lock(&sec_mutex);
    total_requests++;

    IpTracker *t = find_tracker(ip);
    if (!t) {
        t = create_tracker(ip);
        t->request_count = 1;
        pthread_mutex_unlock(&sec_mutex);
        return;
    }

    long now = time(NULL);

    /* Reset window if expired */
    if (now - t->window_start >= RATE_LIMIT_WINDOW) {
        t->window_start = now;
        t->request_count = 1;
        pthread_mutex_unlock(&sec_mutex);
        return;
    }

    t->request_count++;

    /* Check if rate limit exceeded */
    if (t->request_count > RATE_LIMIT_MAX) {
        t->banned_until = now + BAN_DURATION;
        printf("[SECURITY] IP banned for %d seconds: %s (%d requests in %lds)\n",
            BAN_DURATION, ip, t->request_count, now - t->window_start);
    }

    pthread_mutex_unlock(&sec_mutex);
}

double security_get_cpu_usage(void) {
    static long prev_total = 0, prev_idle = 0;

    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0.0;

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0.0;
    }
    fclose(f);

    long user, nice, system, idle, iowait, irq, softirq;
    if (sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq) < 7) {
        return 0.0;
    }

    long total = user + nice + system + idle + iowait + irq + softirq;
    long total_diff = total - prev_total;
    long idle_diff = idle - prev_idle;

    prev_total = total;
    prev_idle = idle;

    if (total_diff == 0) return 0.0;
    return 100.0 * (1.0 - (double)idle_diff / (double)total_diff);
}

static void *cpu_monitor_thread(void *arg) {
    (void)arg;
    /* Initial read to seed the delta */
    security_get_cpu_usage();
    sleep(1);

    while (!should_shutdown) {
        current_cpu = security_get_cpu_usage();

        if (current_cpu > CPU_THRESHOLD) {
            printf("[SECURITY] WARNING: CPU usage at %.1f%% (threshold: %.0f%%)\n",
                   current_cpu, CPU_THRESHOLD);
            /* Check again after a short wait to confirm */
            sleep(2);
            current_cpu = security_get_cpu_usage();
            if (current_cpu > CPU_THRESHOLD) {
                printf("[SECURITY] CRITICAL: Sustained high CPU (%.1f%%). Initiating shutdown!\n",
                       current_cpu);
                should_shutdown = 1;
            }
        }

        sleep(CPU_CHECK_INTERVAL);
    }
    return NULL;
}

void security_start_monitor(void) {
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, cpu_monitor_thread, NULL);
    pthread_attr_destroy(&attr);
    printf("[SECURITY] CPU monitor started (check every %ds)\n", CPU_CHECK_INTERVAL);
}

int security_should_shutdown(void) {
    return should_shutdown;
}

char *security_get_stats(void) {
    pthread_mutex_lock(&sec_mutex);
    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "total_requests", total_requests);
    cJSON_AddNumberToObject(stats, "blocked_requests", blocked_requests);
    cJSON_AddNumberToObject(stats, "tracked_ips", num_tracked);
    cJSON_AddNumberToObject(stats, "cpu_usage", current_cpu);
    cJSON_AddNumberToObject(stats, "cpu_threshold", CPU_THRESHOLD);
    cJSON_AddNumberToObject(stats, "rate_limit", RATE_LIMIT_MAX);
    cJSON_AddNumberToObject(stats, "rate_window", RATE_LIMIT_WINDOW);

    /* List banned IPs */
    cJSON *banned = cJSON_CreateArray();
    long now = time(NULL);
    for (int i = 0; i < num_tracked; i++) {
        if (ip_trackers[i].banned_until > now) {
            cJSON *ban = cJSON_CreateObject();
            cJSON_AddStringToObject(ban, "ip", ip_trackers[i].ip);
            cJSON_AddNumberToObject(ban, "remaining", ip_trackers[i].banned_until - now);
            cJSON_AddItemToArray(banned, ban);
        }
    }
    cJSON_AddItemToObject(stats, "banned_ips", banned);

    char *json = cJSON_PrintUnformatted(stats);
    cJSON_Delete(stats);
    pthread_mutex_unlock(&sec_mutex);
    return json;
}
