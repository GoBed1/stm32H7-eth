/**
 * @file Syslog.h
 * @brief RFC 3164 UDP syslog client for ESP32
 * @author packerlschupfer
 * @license GPL-3.0
 *
 * Lightweight syslog client that sends log messages to remote syslog servers
 * over UDP. Integrates with ESP32-Logger via callback mechanism.
 *
 * Features:
 * - RFC 3164 (BSD syslog) compliant message format
 * - UDP transport (fire-and-forget, low overhead)
 * - Thread-safe with FreeRTOS mutex protection
 * - Configurable log level filtering
 * - Statistics tracking (sent/failed counts)
 * - Supports both WiFi and Ethernet (define SYSLOG_USE_ETHERNET)
 */

#pragma once

#ifndef LOGGER_SYSLOG_H
#define LOGGER_SYSLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

typedef int log_level_t;

#ifndef LOG_LEVEL_NONE
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARNING    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_VERBOSE 5
#endif

// C-friendly facility constants (match Syslog::Facility)
#define SYSLOG_FACILITY_KERN     0
#define SYSLOG_FACILITY_USER     1
#define SYSLOG_FACILITY_MAIL     2
#define SYSLOG_FACILITY_DAEMON   3
#define SYSLOG_FACILITY_AUTH     4
#define SYSLOG_FACILITY_SYSLOG   5
#define SYSLOG_FACILITY_LPR      6
#define SYSLOG_FACILITY_NEWS     7
#define SYSLOG_FACILITY_UUCP     8
#define SYSLOG_FACILITY_CRON     9
#define SYSLOG_FACILITY_AUTHPRIV 10
#define SYSLOG_FACILITY_FTP      11
#define SYSLOG_FACILITY_LOCAL0   16
#define SYSLOG_FACILITY_LOCAL1   17
#define SYSLOG_FACILITY_LOCAL2   18
#define SYSLOG_FACILITY_LOCAL3   19
#define SYSLOG_FACILITY_LOCAL4   20
#define SYSLOG_FACILITY_LOCAL5   21
#define SYSLOG_FACILITY_LOCAL6   22
#define SYSLOG_FACILITY_LOCAL7   23

// Safe to call multiple times. Returns true on success.
bool init_logger(const char* ipstr, int port);
int logger_is_initialized(void);
bool logger_printf(log_level_t level, const char* tag, const char* format, ...);
bool logger_printf_line(log_level_t level, const char* tag, const char* format, ...);
void logger_set_min_level(log_level_t minLevel);
log_level_t logger_get_min_level(void);
#ifdef __cplusplus
}
#endif

#endif /*LOGGER_SYSLOG_H */
