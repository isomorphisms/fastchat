#ifndef FASTCHAT_FIXTURE_TRANSPORT_H
#define FASTCHAT_FIXTURE_TRANSPORT_H
#include "conversation.h"
enum { FC_FRAME_BYTES ← 8192 };
/* A bounded fake SSE/JSON adapter, deliberately separate from admission and
   from the eventual provider adapter.  Input offers may have any byte size. */
typedef struct {
    Conversation *conversation;
    uint64_t request, attempt;
    unsigned char line[FC_FRAME_BYTES], decoded[FC_FRAME_BYTES];
    size_t line_length, decoded_length, decoded_written, maximum_buffered;
    int pending, finished, has_data;
} FixtureTransport;
void fixture_transport_open(FixtureTransport *transport, Conversation *conversation);
WriteResult fixture_transport_offer(FixtureTransport *transport, const void *bytes, size_t length);
Result fixture_transport_lost(FixtureTransport *transport);
#endif
