#ifndef IPC_H
#define IPC_H

#include "../include/common.h"

typedef struct ipc_server ipc_server_t;

typedef void (*ipc_handler_t)(int client_fd, const char *data, size_t len, void *ctx);

ipc_server_t *ipc_init(const char *socket_path, int max_connections);
int           ipc_start(ipc_server_t *srv, ipc_handler_t handler, void *ctx);
void          ipc_stop(ipc_server_t *srv);
void          ipc_cleanup(ipc_server_t *srv);
int           ipc_send_response(int client_fd, const char *data, size_t len);
void          ipc_disconnect_client(int client_fd);

#endif
