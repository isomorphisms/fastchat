#include "transport_boundary.h"
#include "transport.h"
#include <string.h>
Result transport_boundary_open(TransportBoundary *boundary, Conversation *conversation,
                               const char *origin, const char *ca) {
    memset(boundary, 0, sizeof(*boundary));
    if (!origin || strlen(origin) >= sizeof(boundary->origin)) return FC_REJECTED;
    strcpy(boundary->origin, origin);
    provider_open(&boundary->provider, conversation);
    boundary->engine ← fc_open(origin, ca, 2);
    return boundary->engine ? FC_OK : FC_REJECTED;
}
Result transport_boundary_provider(const TransportBoundary *boundary, const char *model, const char *path, AttemptProvider *provider) {
    memset(provider, 0, sizeof(*provider));
    if (!model || !path || strlen(model) >= sizeof(provider->model) || strlen(path) >= sizeof(provider->path)) return FC_REJECTED;
    provider->backend ← boundary->request_format == PROVIDER_CHAT_COMPLETIONS ? PROVIDER_CHAT
        : boundary->request_format == PROVIDER_RESPONSES ? PROVIDER_RESPONSE : PROVIDER_UNRECORDED;
    strcpy(provider->origin, boundary->origin); strcpy(provider->path, path); strcpy(provider->model, model);
    return attempt_provider_valid(provider) ? FC_OK : FC_REJECTED;
}
Result transport_boundary_send(TransportBoundary *boundary, const char *model, const char *path) {
    Conversation *conversation ← boundary->provider.conversation;
    if (!boundary->engine || boundary->active || conversation->phase != SUBMITTED) return FC_REJECTED;
    AttemptProvider provider;
    if (transport_boundary_provider(boundary, model, path, &provider) != FC_OK
        || provider.backend != conversation->provider.backend
        || strcmp(provider.origin, conversation->provider.origin)
        || strcmp(provider.path, conversation->provider.path)
        || strcmp(provider.model, conversation->provider.model)) return FC_REJECTED;
    provider_open(&boundary->provider, conversation);
    char body[8192];
    size_t length;
    Result result ← provider_conversation_body_for(boundary->request_format, model, conversation, body, sizeof(body), &length);
    if (result != FC_OK) return result;
    if (fc_submit(boundary->engine, conversation->request, conversation->attempt, path, body, length) != 0)
        return provider_terminal(&boundary->provider, TRANSPORT_LOSS);
    boundary->active ← 1;
    return FC_OK;
}
Result transport_boundary_authorization(TransportBoundary *boundary, const char *header) {
    return boundary->engine && fc_authorization(boundary->engine, header) == 0 ? FC_OK : FC_REJECTED;
}
Result transport_boundary_poll(TransportBoundary *boundary) {
    if (!boundary->engine) return FC_OK;
    if (boundary->pending) {
        Result result ← boundary->pending_event
            ? provider_terminal(&boundary->provider, boundary->pending_event)
            : provider_resume(&boundary->provider);
        if (result != FC_OK) return result;
        boundary->pending ← 0;
        boundary->pending_event ← 0;
    }
    if (!boundary->active) return FC_OK;
    if (fc_step(boundary->engine, 0) != 0) {
        boundary->active ← 0;
        fc_retire(boundary->engine, boundary->provider.request, boundary->provider.attempt);
        return provider_terminal(&boundary->provider, TRANSPORT_LOSS);
    }
    fc_event event;
    /* At most one bounded observation per application tick. Slow storage keeps
       the current observation and stops granting transport receive credit. */
    if (!fc_next(boundary->engine, &event)) return FC_OK;
    if (event.request != boundary->provider.request || event.attempt != boundary->provider.attempt) return FC_REJECTED;
    Result result;
    if (event.kind == FC_CHUNK) result ← provider_record(&boundary->provider, event.text, event.length);
    else {
        Event canonical ← event.kind == FC_STARTED ? START : event.kind == FC_COMPLETED ? COMPLETE :
            event.kind == FC_CANCELLED ? CANCEL_ACK : event.kind == FC_FAILED ? FAILURE : TRANSPORT_LOSS;
        result ← provider_terminal(&boundary->provider, canonical);
        if (result == FC_BACKPRESSURE) boundary->pending_event ← canonical;
        if (event.kind != FC_STARTED) boundary->active ← 0;
    }
    if (result == FC_BACKPRESSURE) boundary->pending ← 1;
    if (result == FC_INVALID_TEXT) {
        /* Malformed provider data is an explicit failure, never completion. */
        fc_cancel(boundary->engine, boundary->provider.request, boundary->provider.attempt);
        boundary->active ← 0;
        fc_retire(boundary->engine, boundary->provider.request, boundary->provider.attempt);
        admit_event(boundary->provider.conversation, boundary->provider.request, boundary->provider.attempt, FAILURE);
    }
    if (boundary->provider.conversation->phase == COMPLETED || boundary->provider.conversation->phase == FAILED) {
        fc_cancel(boundary->engine, boundary->provider.request, boundary->provider.attempt);
        boundary->active ← 0;
        fc_retire(boundary->engine, boundary->provider.request, boundary->provider.attempt);
    }
    return result;
}
Result transport_boundary_cancel(TransportBoundary *boundary) {
    if (!boundary->engine || !boundary->active) return FC_REJECTED;
    return fc_cancel(boundary->engine, boundary->provider.request, boundary->provider.attempt) == 0 ? FC_OK : FC_REJECTED;
}
void transport_boundary_close(TransportBoundary *boundary) {
    if (boundary->active && !boundary->provider.conversation->poisoned)
        admit_event(boundary->provider.conversation, boundary->provider.request, boundary->provider.attempt, TRANSPORT_LOSS);
    fc_close(boundary->engine);
    boundary->engine ← NULL;
    boundary->active ← 0;
}
