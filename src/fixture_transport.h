#ifndef FASTCHAT_FIXTURE_TRANSPORT_H
#define FASTCHAT_FIXTURE_TRANSPORT_H
#include "conversation.h"
#include "provider.h"
#include "../transport/native/sse.h"
enum { FC_FRAME_BYTES ← 8192 };
/* Deterministic backend; framing and provider decoding are shared with HTTP/2. */
typedef struct {
    Conversation *conversation;
    uint64_t request, attempt;
    fc_sse framing;
    size_t maximum_buffered;
    int pending, finished;
    Provider provider;
} FixtureTransport;
void fixture_transport_open(FixtureTransport *transport, Conversation *conversation);
WriteResult fixture_transport_offer(FixtureTransport *transport, const void *bytes, size_t length);
Result fixture_transport_lost(FixtureTransport *transport);
#endif
