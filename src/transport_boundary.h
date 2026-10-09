#ifndef FASTCHAT_TRANSPORT_BOUNDARY_H
#define FASTCHAT_TRANSPORT_BOUNDARY_H
#include "provider.h"
/* Opaque engine identity. No socket, TLS, H2 or wire stream enters this API. */
typedef struct fc_transport fc_transport;
typedef struct {
    Provider provider;
    fc_transport *engine;
    int active, pending;
    Event pending_event;
} TransportBoundary;
Result transport_boundary_open(TransportBoundary *, Conversation *, const char *origin, const char *ca);
Result transport_boundary_send(TransportBoundary *, const char *model, const char *path);
Result transport_boundary_authorization(TransportBoundary *, const char *header);
Result transport_boundary_poll(TransportBoundary *);
Result transport_boundary_cancel(TransportBoundary *);
void transport_boundary_close(TransportBoundary *);
#endif
