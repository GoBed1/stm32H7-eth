/**
 * @file Syslog.c
 * @brief RFC 3164 UDP syslog client implementation (pure C)
 */

#include "syslog.h"

#include "main.h"
#include "stm32h7xx_hal.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lwip.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "board.h"
#if defined(LOCK_TCPIP_CORE) && defined(UNLOCK_TCPIP_CORE)
#define SYSLOG_LWIP_LOCK()   LOCK_TCPIP_CORE()
#define SYSLOG_LWIP_UNLOCK() UNLOCK_TCPIP_CORE()
#else
#define SYSLOG_LWIP_LOCK()   do {} while (0)
#define SYSLOG_LWIP_UNLOCK() do {} while (0)
#endif

/* Compile-time constant for static buffers */
#define SYSLOG_MAX_MESSAGE_SIZE 1024
static const TickType_t MUTEX_TIMEOUT_MS = 100;

typedef struct {
    ip_addr_t server;
    uint16_t port;
    int facility;
    log_level_t min_level;
    char hostname[64];
    char app_name[48];
    bool initialized;
    struct udp_pcb* udp;
    SemaphoreHandle_t mutex;
    uint32_t send_count;
    uint32_t failed_count;
} Syslog_t;

/* Global syslog instance (simpler than placement-storage singleton) */
static Syslog_t logger_syslog = {
    .server = {0},
    .port = 514,
    .facility = SYSLOG_FACILITY_USER,
    .min_level = LOG_LEVEL_VERBOSE,
    .hostname = "craner",
    .app_name = "logger",
    .initialized = false,
    .udp = NULL,
    .mutex = NULL,
    .send_count = 0,
    .failed_count = 0
};

static inline Syslog_t* get_logger_obj(void)
{
    return &logger_syslog;
}

static int syslog_get_severity(const Syslog_t* s, log_level_t level)
{
    (void)s;
    switch (level) {
        case LOG_LEVEL_NONE:    return 0;
        case LOG_LEVEL_ERROR:   return 3;
        case LOG_LEVEL_WARNING: return 4;
        case LOG_LEVEL_INFO:    return 6;
        case LOG_LEVEL_DEBUG:   return 7;
        case LOG_LEVEL_VERBOSE: return 7;
        default: return 6;
    }
}

static int syslog_get_priority(const Syslog_t* s, log_level_t level)
{
    return (s->facility * 8) + syslog_get_severity(s, level);
}

static size_t syslog_format_msg(Syslog_t* s, char* buffer, size_t bufferSize,
                            log_level_t level, const char* tag, const char* message)
{
    if (!buffer || bufferSize < 64) return 0;
    char timestamp[20];
    board_get_timestamp(timestamp, sizeof(timestamp));
    int priority = syslog_get_priority(s, level);
    int written = snprintf(buffer, bufferSize,
                          "<%d>%s %s %s[%s]: %s",
                          priority,
                          timestamp,
                          s->hostname,
                          s->app_name,
                          tag ? tag : "unknown",
                          message ? message : "");
    if (written < 0 || written >= (int)bufferSize) return 0;
    return (size_t)written;
}

bool init_logger(const char* ipstr, int port)
{
    if (!ipstr) {
        printf("ERROR: init_logger_params: NULL ipstr\n");
        return false;
    }
    if (port <= 0 || port > 65535) {
        printf("ERROR: init_logger_params: invalid port %d\n", port);
        return false;
    }

    ip_addr_t server;
    if (!ipaddr_aton(ipstr, &server)) {
        printf("ERROR: init_logger_params: invalid ip string '%s'\n", ipstr);
        return false;
    }

    Syslog_t* s = get_logger_obj();
    if (!s) {
        printf("ERROR: Failed to initialize syslog!\n");
        return false;
    }

    /* begin (configure server/port/facility) */
    if (!s->mutex) {
        s->mutex = xSemaphoreCreateMutex();
        if (!s->mutex) {
            printf("ERROR: Failed to create syslog mutex\n");
            return false;
        }
    }

    if (xSemaphoreTake(s->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        printf("ERROR: Failed to take syslog mutex\n");
        return false;
    }

    /* cleanup existing */
    if (s->initialized) {
        if (s->udp) {
            SYSLOG_LWIP_LOCK();
            udp_remove(s->udp);
            SYSLOG_LWIP_UNLOCK();
            s->udp = NULL;
        }
        s->initialized = false;
    }

    s->server = server;
    s->port = (uint16_t)port;

    SYSLOG_LWIP_LOCK();
    s->udp = udp_new();
    if (s->udp) {
        err_t bindErr = udp_bind(s->udp, IP_ADDR_ANY, 0);
        if (bindErr != ERR_OK) {
            udp_remove(s->udp);
            s->udp = NULL;
        }
    }
    SYSLOG_LWIP_UNLOCK();

    if (!s->udp) {
        xSemaphoreGive(s->mutex);
        printf("ERROR: Failed to create UDP PCB for syslog\n");
        return false;
    }

    s->initialized = true;
    xSemaphoreGive(s->mutex);
    char ipbuf[48];
    printf("Syslog initialized: %s:%u\n", ipaddr_ntoa_r(&s->server, ipbuf, sizeof(ipbuf)), (unsigned)s->port);
    return true;
}

bool logger_output(log_level_t level, const char* tag, const char* message)
{
    Syslog_t* s = get_logger_obj();

    /* If syslog is initialized, send via UDP. Otherwise fallback to printf. */
    if (s && s->initialized && s->udp && s->mutex) {
        if (level > s->min_level) return true;

        if (xSemaphoreTake(s->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
            s->failed_count++;
            return false;
        }

        /* double-check state after taking mutex */
        if (!s->initialized || !s->udp) {
            s->failed_count++;
            xSemaphoreGive(s->mutex);
            /* fallthrough to printf fallback below */
        } else {
            char buffer[SYSLOG_MAX_MESSAGE_SIZE];
            size_t msgLen = syslog_format_msg(s, buffer, sizeof(buffer), level, tag, message);
            if (msgLen == 0) {
                s->failed_count++;
                xSemaphoreGive(s->mutex);
                return false;
            }

            bool ok = false;
            SYSLOG_LWIP_LOCK();
            struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)msgLen, PBUF_RAM);
            if (p) {
                if (pbuf_take(p, buffer, msgLen) == ERR_OK) {
                    err_t err = udp_sendto(s->udp, p, &s->server, s->port);
                    ok = (err == ERR_OK);
                }
                pbuf_free(p);
            }
            SYSLOG_LWIP_UNLOCK();

            if (!ok) {
                s->failed_count++;
                xSemaphoreGive(s->mutex);
                return false;
            }

            s->send_count++;
            xSemaphoreGive(s->mutex);
            return true;
        }
    }

    printf("%s\n",message);
    return true;
}

bool logger_printf(log_level_t level, const char* tag, const char* format, ...)
{
    if (!format) return false;
    char msg[512];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    if (n < 0) return false;
    if (n >= (int)sizeof(msg)) msg[sizeof(msg)-1] = '\0';

    return logger_output(level, tag ? tag : "printf", msg);
}

bool logger_printf_line(log_level_t level, const char* tag, const char* format, ...)
{
    static char line_buf[SYSLOG_MAX_MESSAGE_SIZE];
    static size_t line_len = 0;
    static log_level_t cur_level = LOG_LEVEL_DEBUG;
    static char cur_tag[48] = {0};
    static SemaphoreHandle_t line_mutex = NULL;

    if (!format) return false;

    if (!line_mutex) {
        line_mutex = xSemaphoreCreateMutex();
    }

    char tmp[256];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);

    if (n < 0) return false;
    if (n >= (int)sizeof(tmp)) tmp[sizeof(tmp)-1] = '\0';

    if (line_mutex && xSemaphoreTake(line_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return false;
    }

    bool all_ok = true;
    const char* p = tmp;
    while (*p) {
        const char* nl = strpbrk(p, "\r\n");
        size_t chunk_len = nl ? (size_t)(nl - p) : strlen(p);

        if (line_len > 0) {
            if ((tag && cur_tag[0] && strncmp(cur_tag, tag, sizeof(cur_tag)) != 0) || level != cur_level) {
                line_buf[line_len] = '\0';
                all_ok = all_ok && logger_output(cur_level, cur_tag[0] ? cur_tag : (tag ? tag : "printf"), line_buf);
                line_len = 0;
                cur_tag[0] = '\0';
            }
        }

        if (line_len == 0) {
            cur_level = level;
            const char* use_tag = tag ? tag : "printf";
            strncpy(cur_tag, use_tag, sizeof(cur_tag)-1);
            cur_tag[sizeof(cur_tag)-1] = '\0';
        }

        size_t space = sizeof(line_buf) - 1 - line_len;
        if (chunk_len > space) {
            if (line_len > 0) {
                line_buf[line_len] = '\0';
                all_ok = all_ok && logger_output(cur_level, cur_tag, line_buf);
                line_len = 0;
            }
            while (chunk_len > sizeof(line_buf) - 1) {
                size_t seg = sizeof(line_buf) - 1;
                memcpy(line_buf, p, seg);
                line_buf[seg] = '\0';
                all_ok = all_ok && logger_output(level, tag ? tag : "printf", line_buf);
                p += seg;
                chunk_len -= seg;
            }
        }

        memcpy(line_buf + line_len, p, chunk_len);
        line_len += chunk_len;
        p += chunk_len;

        if (nl) {
            line_buf[line_len] = '\0';
            all_ok = all_ok && logger_output(cur_level, cur_tag, line_buf);
            line_len = 0;
            cur_tag[0] = '\0';

            if (nl[0] == '\r' && nl[1] == '\n') {
                p = nl + 2;
            } else if (nl[0] == '\n' && nl[1] == '\r') {
                p = nl + 2;
            } else {
                p = nl + 1;
            }
        }
    }

    if (line_mutex) {
        xSemaphoreGive(line_mutex);
    }
    return all_ok;
}

void logger_set_min_level(log_level_t min_level)
{
    Syslog_t* s = get_logger_obj();
    if (!s) return;
    if (!s->mutex) return;
    if (xSemaphoreTake(s->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return;
    s->min_level = min_level;
    xSemaphoreGive(s->mutex);
}

log_level_t logger_get_min_level(void)
{
    Syslog_t* s = get_logger_obj();
    if (!s) return LOG_LEVEL_VERBOSE;
    return s->min_level;
}

void logger_get_stats(uint32_t* sent, uint32_t* failed)
{
    if (!sent || !failed) return;
    Syslog_t* s = get_logger_obj();
    if (!s) return;
    if (!s->mutex) return;
    if (xSemaphoreTake(s->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return;
    *sent = s->send_count;
    *failed = s->failed_count;
    xSemaphoreGive(s->mutex);
}

void logger_reset_stats(void)
{
    Syslog_t* s = get_logger_obj();
    if (!s) return;
    if (!s->mutex) return;
    if (xSemaphoreTake(s->mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) return;
    s->send_count = 0;
    s->failed_count = 0;
    xSemaphoreGive(s->mutex);
}

int logger_is_initialized(void)
{
    Syslog_t* s = get_logger_obj();
    return (s && s->initialized) ? 1 : 0;
}
