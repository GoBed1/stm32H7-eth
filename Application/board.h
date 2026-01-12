#ifndef APPLICATION_BOARD_H
#define APPLICATION_BOARD_H
#ifdef __cplusplus
 extern "C" {
#endif

#include <stddef.h>

#define SYSLOG_SERVER_IP "192.168.1.1"
#define SYSLOG_SERVER_PORT 514
#define DNS_SERVER_IP1 "192.168.1.1"
#define DNS_SERVER_IP2 "8.8.8.8"
#define NTP_SERVER_IP1 "ntp.towercrane.lan"
#define NTP_SERVER_IP2 "pool.ntp.org"

char* board_get_timestamp(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
#endif /*APPLICATION_BOARD_H */
