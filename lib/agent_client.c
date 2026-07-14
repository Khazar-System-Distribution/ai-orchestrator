#include "agent_client.h"
#include "../logger/logger.h"
#include "../registry/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>

#define MODULE "agent_client"
#define HEARTBEAT_INTERVAL 30

struct agent_client {
    char socket_path[256];
    int  fd;
    char name[64];
    time_t last_heartbeat;
    bool registered;
};

agent_client_t *agent_client_init(const char *orchestrator_socket) {
    agent_client_t *ac = calloc(1, sizeof(agent_client_t));
    if (!ac) return NULL;

    snprintf(ac->socket_path, sizeof(ac->socket_path), "%s",
             orchestrator_socket ? orchestrator_socket : SOCKET_PATH_DEFAULT);
    ac->fd = -1;

    return ac;
}

int agent_client_connect(agent_client_t *ac) {
    if (!ac) return -1;

    if (ac->fd >= 0) close(ac->fd);

    ac->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ac->fd < 0) {
        log_error(MODULE, "socket creation failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", ac->socket_path);

    if (connect(ac->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error(MODULE, "connect to %s failed: %s", ac->socket_path, strerror(errno));
        close(ac->fd);
        ac->fd = -1;
        return -1;
    }

    log_info(MODULE, "connected to orchestrator at %s", ac->socket_path);
    return 0;
}

int agent_client_register(agent_client_t *ac, const char *name, const char *version, const char **capabilities, int cap_count) {
    if (!ac || !name) return -1;

    strncpy(ac->name, name, sizeof(ac->name) - 1);

    char msg[4096];
    int offset = snprintf(msg, sizeof(msg),
        "{\"type\":\"register\",\"name\":\"%s\",\"version\":\"%s\",\"capabilities\":[",
        name, version ? version : "0.1");

    for (int i = 0; i < cap_count && i < MAX_CAPABILITIES; i++) {
        offset += snprintf(msg + offset, sizeof(msg) - offset,
            "%s\"%s\"", i > 0 ? "," : "", capabilities[i]);
    }
    snprintf(msg + offset, sizeof(msg) - offset, "]}");

    ssize_t sent = write(ac->fd, msg, strlen(msg));
    if (sent < 0) {
        log_error(MODULE, "register write failed: %s", strerror(errno));
        return -1;
    }

    char resp[1024];
    ssize_t n = read(ac->fd, resp, sizeof(resp) - 1);
    if (n > 0) {
        resp[n] = '\0';
        log_debug(MODULE, "register response: %s", resp);
    }

    ac->registered = true;
    ac->last_heartbeat = time(NULL);
    log_info(MODULE, "agent '%s' registered", name);
    return 0;
}

int agent_client_send_heartbeat(agent_client_t *ac) {
    if (!ac || ac->fd < 0) return -1;

    time_t now = time(NULL);
    if (now - ac->last_heartbeat < HEARTBEAT_INTERVAL)
        return 0;

    char msg[256];
    snprintf(msg, sizeof(msg), "{\"type\":\"heartbeat\",\"agent\":\"%s\"}", ac->name);

    write(ac->fd, msg, strlen(msg));
    ac->last_heartbeat = now;

    return 0;
}

int agent_client_read_request(agent_client_t *ac, char *buf, size_t buf_size, int timeout_ms) {
    if (!ac || ac->fd < 0 || !buf) return -1;

    struct pollfd pfd = {.fd = ac->fd, .events = POLLIN};
    int ret = poll(&pfd, 1, timeout_ms);

    if (ret < 0) {
        log_error(MODULE, "poll error: %s", strerror(errno));
        return -1;
    }

    if (ret == 0) return 0;

    if (!(pfd.revents & POLLIN)) {
        if (pfd.revents & (POLLHUP | POLLERR)) {
            log_warn(MODULE, "orchestrator disconnected");
            return -1;
        }
        return 0;
    }

    ssize_t n = read(ac->fd, buf, buf_size - 1);
    if (n <= 0) {
        log_warn(MODULE, "read failed or connection closed");
        return -1;
    }

    buf[n] = '\0';
    return (int)n;
}

int agent_client_send_response(agent_client_t *ac, const char *status, const char *payload) {
    if (!ac || ac->fd < 0) return -1;

    char resp[4096];
    snprintf(resp, sizeof(resp),
        "{\"status\":\"%s\",\"payload\":{\"message\":\"%s\"}}",
        status ? status : "success",
        payload ? payload : "");

    ssize_t sent = write(ac->fd, resp, strlen(resp));
    if (sent < 0) {
        log_error(MODULE, "response write failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

void agent_client_disconnect(agent_client_t *ac) {
    if (!ac) return;

    if (ac->fd >= 0) {
        close(ac->fd);
        ac->fd = -1;
    }

    ac->registered = false;
    log_info(MODULE, "disconnected from orchestrator");
}

void agent_client_cleanup(agent_client_t *ac) {
    if (!ac) return;
    agent_client_disconnect(ac);
    free(ac);
}
