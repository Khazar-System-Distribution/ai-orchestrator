#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <sys/types.h>

#define MAX_CONNECTIONS     256
#define MAX_MESSAGE_SIZE    65536
#define MAX_AGENTS          64
#define MAX_SESSIONS        1024
#define MAX_WORKER_THREADS  4
#define SOCKET_PATH_DEFAULT "/run/ai-orchestrator.sock"
#define REQUEST_TIMEOUT_MS  3000
#define SOCKET_PERMISSIONS  0660

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

typedef enum {
    STATUS_SUCCESS,
    STATUS_ERROR
} status_t;

typedef enum {
    ERR_NONE = 0,
    ERR_AGENT_TIMEOUT,
    ERR_AGENT_DISCONNECTED,
    ERR_POLICY_DENIED,
    ERR_INTERNAL,
    ERR_INVALID_REQUEST,
    ERR_AGENT_UNAVAILABLE,
    ERR_SYSTEM
} error_code_t;

typedef enum {
    PRIORITY_CRITICAL = 0,
    PRIORITY_HIGH = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_BACKGROUND = 3
} priority_t;

typedef struct {
    uint64_t id;
    char     query[MAX_MESSAGE_SIZE];
    char     client_addr[64];
    time_t   received_at;
    priority_t priority;
    int      client_fd;
} request_t;

typedef struct {
    uint64_t id;
    status_t status;
    char     payload[MAX_MESSAGE_SIZE];
    error_code_t error_code;
} response_t;

static inline const char *error_code_str(error_code_t code) {
    switch (code) {
        case ERR_NONE:              return "NONE";
        case ERR_AGENT_TIMEOUT:     return "AGENT_TIMEOUT";
        case ERR_AGENT_DISCONNECTED:return "AGENT_DISCONNECTED";
        case ERR_POLICY_DENIED:     return "POLICY_DENIED";
        case ERR_INTERNAL:          return "INTERNAL_ERROR";
        case ERR_INVALID_REQUEST:   return "INVALID_REQUEST";
        case ERR_AGENT_UNAVAILABLE: return "AGENT_UNAVAILABLE";
        case ERR_SYSTEM:            return "SYSTEM_ERROR";
        default:                    return "UNKNOWN";
    }
}

#endif
