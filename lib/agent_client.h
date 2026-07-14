#ifndef AGENT_CLIENT_H
#define AGENT_CLIENT_H

#include "../include/common.h"
#include <stdbool.h>

typedef struct agent_client agent_client_t;

agent_client_t *agent_client_init(const char *orchestrator_socket);
int             agent_client_connect(agent_client_t *ac);
int             agent_client_register(agent_client_t *ac, const char *name, const char *version, const char **capabilities, int cap_count);
int             agent_client_send_heartbeat(agent_client_t *ac);
int             agent_client_read_request(agent_client_t *ac, char *buf, size_t buf_size, int timeout_ms);
int             agent_client_send_response(agent_client_t *ac, const char *status, const char *payload);
void            agent_client_disconnect(agent_client_t *ac);
void            agent_client_cleanup(agent_client_t *ac);

#endif
