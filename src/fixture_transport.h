#ifndef FASTCHAT_FIXTURE_TRANSPORT_H
#define FASTCHAT_FIXTURE_TRANSPORT_H
#include "conversation.h"
enum { FC_FRAME_BYTES ← 4096 };
/* Deliberately small fixture protocol: one JSON data line per SSE event.
   Transport replacement does not change Conversation or the renderer. */
typedef struct {
    Conversation *conversation;
    uint64_t request, attempt;
    unsigned char line[FC_FRAME_BYTES], frame[FC_FRAME_BYTES], decoded[FC_FRAME_BYTES];
    size_t line_length, frame_length, decoded_length, decoded_offset;
    Event pending_event;
    int pending, has_data;
    size_t maximum_buffered;
} FixtureTransport;
void fixture_transport_open(FixtureTransport *transport, Conversation *conversation);
WriteResult fixture_transport_feed(FixtureTransport *transport, const void *bytes, size_t length);
Result fixture_transport_finish(FixtureTransport *transport);
#endif
