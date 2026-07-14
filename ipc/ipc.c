#define _GNU_SOURCE
#include "ipc.h"
#include "../logger/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

static int set_nonblocking(int fd) __attribute__((unused));
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

#define MODULE "ipc"
#define MAX_EVENTS 256
#define BUFFER_SIZE MAX_MESSAGE_SIZE

struct ipc_server {
    char socket_path[256];
    int  max_connections;
    int  server_fd;
    int  epoll_fd;
    bool running;
    ipc_handler_t handler;
    void *handler_ctx;
    pthread_t accept_thread;
};

typedef struct {
    int  fd;
    char buf[BUFFER_SIZE];
    size_t len;
} client_ctx_t;

ipc_server_t *ipc_init(const char *socket_path, int max_connections) {
    ipc_server_t *srv = calloc(1, sizeof(ipc_server_t));
    if (!srv) return NULL;

    strncpy(srv->socket_path, socket_path ? socket_path : SOCKET_PATH_DEFAULT, sizeof(srv->socket_path) - 1);
    srv->max_connections = max_connections > 0 ? max_connections : MAX_CONNECTIONS;
    srv->server_fd = -1;
    srv->epoll_fd = -1;
    srv->running = false;

    return srv;
}

int ipc_start(ipc_server_t *srv, ipc_handler_t handler, void *ctx) {
    if (!srv) return -1;

    srv->handler = handler;
    srv->handler_ctx = ctx;

    unlink(srv->socket_path);

    srv->server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (srv->server_fd < 0) {
        log_error(MODULE, "socket creation failed: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", srv->socket_path);

    if (bind(srv->server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log_error(MODULE, "bind failed: %s", strerror(errno));
        close(srv->server_fd);
        return -1;
    }

    chmod(srv->socket_path, SOCKET_PERMISSIONS);

    if (listen(srv->server_fd, srv->max_connections) < 0) {
        log_error(MODULE, "listen failed: %s", strerror(errno));
        close(srv->server_fd);
        unlink(srv->socket_path);
        return -1;
    }

    srv->epoll_fd = epoll_create1(0);
    if (srv->epoll_fd < 0) {
        log_error(MODULE, "epoll creation failed: %s", strerror(errno));
        close(srv->server_fd);
        unlink(srv->socket_path);
        return -1;
    }

    struct epoll_event ev = {.events = EPOLLIN, .data.fd = srv->server_fd};
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->server_fd, &ev) < 0) {
        log_error(MODULE, "epoll_ctl failed: %s", strerror(errno));
        close(srv->epoll_fd);
        close(srv->server_fd);
        unlink(srv->socket_path);
        return -1;
    }

    srv->running = true;
    log_info(MODULE, "server listening on %s", srv->socket_path);

    struct epoll_event events[MAX_EVENTS];

    while (srv->running) {
        int nfds = epoll_wait(srv->epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            log_error(MODULE, "epoll_wait error: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == srv->server_fd) {
                struct sockaddr_un client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept4(srv->server_fd, (struct sockaddr*)&client_addr, &client_len, SOCK_NONBLOCK);
                if (client_fd < 0) {
                    log_error(MODULE, "accept failed: %s", strerror(errno));
                    continue;
                }

                struct epoll_event cev = {.events = EPOLLIN | EPOLLRDHUP, .data.fd = client_fd};
                if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, client_fd, &cev) < 0) {
                    close(client_fd);
                    continue;
                }

                log_debug(MODULE, "client connected: fd=%d", client_fd);
            } else {
                int client_fd = events[i].data.fd;

                if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    log_debug(MODULE, "client disconnected: fd=%d", client_fd);
                    continue;
                }

                char buf[BUFFER_SIZE];
                ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
                if (n <= 0) {
                    epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    log_debug(MODULE, "client disconnected: fd=%d", client_fd);
                } else {
                    buf[n] = '\0';
                    if (srv->handler) {
                        srv->handler(client_fd, buf, (size_t)n, srv->handler_ctx);
                    }
                }
            }
        }
    }

    return 0;
}

void ipc_stop(ipc_server_t *srv) {
    if (srv) srv->running = false;
}

void ipc_cleanup(ipc_server_t *srv) {
    if (!srv) return;

    srv->running = false;

    if (srv->epoll_fd >= 0) close(srv->epoll_fd);
    if (srv->server_fd >= 0) close(srv->server_fd);

    unlink(srv->socket_path);
    free(srv);
    log_info(MODULE, "server cleaned up");
}

int ipc_send_response(int client_fd, const char *data, size_t len) {
    if (client_fd < 0 || !data) return -1;

    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(client_fd, data + sent, len - sent);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

void ipc_disconnect_client(int client_fd) {
    if (client_fd >= 0) close(client_fd);
}
