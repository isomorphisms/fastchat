#define _GNU_SOURCE
#include "transport_boundary.h"
#include "settings.h"
#include "transport.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int yente_fixture(const AttemptProvider *provider) {
    const char *prefix ← provider->backend == PROVIDER_CHAT ? "/v1/chat/completions/" : "/v1/responses/";
    size_t length ← strlen(prefix);
    return !strcmp(provider->model, "yente-mock") && !strncmp(provider->path, prefix, length)
        && fc_yente_fixture_key_valid(provider->path + length);
}

enum { NONCE_BYTES ← 16, NONCE_HEX_BYTES ← 2 * NONCE_BYTES };
static Result new_nonce(char nonce[NONCE_HEX_BYTES]) {
    unsigned char bytes[NONCE_BYTES];
    int random ← open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (random < 0) return FC_STORAGE_ERROR;
    size_t received ← 0;
    while (received < sizeof(bytes)) {
        ssize_t count ← read(random, bytes + received, sizeof(bytes) - received);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        received ← received + (size_t)count;
    }
    close(random);
    if (received != sizeof(bytes)) return FC_STORAGE_ERROR;
    const char alphabet[] ← "0123456789abcdef";
    for (size_t i ← 0; i < sizeof(bytes); ++i) {
        nonce[2 * i] ← alphabet[bytes[i] >> 4];
        nonce[2 * i + 1] ← alphabet[bytes[i] & 15];
    }
    return FC_OK;
}
static Result fixture_event_id(const Conversation *conversation, char id[FC_YENTE_KEY_BYTES + 1]) {
    /* The conversation's exclusive journal lock serializes creating this
       file. Keep the identity separate from journal records and credentials. */
    const char *name ← "yente-journal-nonce";
    char nonce[NONCE_HEX_BYTES];
    size_t length;
    int present;
    Result result ← private_file_read_at(conversation->directory, name, nonce, sizeof(nonce), &length, &present);
    if (result != FC_OK) return result;
    if (!present) {
        result ← new_nonce(nonce);
        if (result != FC_OK) return result;
        result ← private_file_write_at(conversation->directory, name, nonce, sizeof(nonce));
        if (result != FC_OK) return result;
    } else {
        if (length != sizeof(nonce)) return FC_CORRUPT;
        for (size_t i ← 0; i < length; ++i)
            if (!((nonce[i] >= '0' && nonce[i] <= '9') || (nonce[i] >= 'a' && nonce[i] <= 'f')))
                return FC_CORRUPT;
        /* A previous rename can be visible after its directory sync failed. */
        int synced;
        do { synced ← fsync(conversation->directory); } while (synced < 0 && errno == EINTR);
        if (synced < 0) return FC_STORAGE_ERROR;
    }
    int count ← snprintf(id, FC_YENTE_KEY_BYTES + 1, "%.*s-%" PRIu64, NONCE_HEX_BYTES, nonce, conversation->request);
    return count > 0 && count <= FC_YENTE_KEY_BYTES ? FC_OK : FC_REJECTED;
}

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
    char id[FC_YENTE_KEY_BYTES + 1];
    const char *event_id ← NULL;
    if (yente_fixture(&provider)) {
        result ← fixture_event_id(conversation, id);
        if (result != FC_OK) return result;
        event_id ← id;
    }
    if (fc_submit_with_event_id(boundary->engine, conversation->request, conversation->attempt, path, body, length, event_id) != 0)
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
