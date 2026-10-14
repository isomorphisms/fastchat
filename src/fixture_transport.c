#include "fixture_transport.h"
#include <string.h>
static Result resume(FixtureTransport *transport) {
    Result result ← provider_resume(&transport->provider);
    transport->pending ← result == FC_BACKPRESSURE;
    return result;
}
static Result dispatch(FixtureTransport *transport, const unsigned char *bytes, size_t length) {
    Event event;
    if (length == 7 && !memcmp(bytes, "[START]", 7)) event ← START;
    else if (length == 6 && !memcmp(bytes, "[DONE]", 6)) event ← COMPLETE;
    else if (length == 6 && !memcmp(bytes, "[LOST]", 6)) event ← TRANSPORT_LOSS;
    else if (length == 8 && !memcmp(bytes, "[FAILED]", 8)) event ← FAILURE;
    else {
        Provider *provider ← &transport->provider;
        Result result ← provider_decode_fixture_text(bytes, length, provider->pending, &provider->length);
        if (result == FC_OK) result ← resume(transport);
        else result ← provider_record(provider, (const char *)bytes, length);
        transport->pending ← result == FC_BACKPRESSURE;
        Phase phase ← transport->conversation->phase;
        if (result == FC_OK && (phase == COMPLETED || phase == FAILED)) transport->finished ← 1;
        return result;
    }
    Result result ← provider_terminal(&transport->provider, event);
    if (result == FC_OK && event != START) transport->finished ← 1;
    return result;
}
void fixture_transport_open(FixtureTransport *transport, Conversation *conversation) {
    memset(transport, 0, sizeof(*transport));
    transport->conversation ← conversation;
    transport->request ← conversation->request;
    transport->attempt ← conversation->attempt;
    provider_open(&transport->provider, conversation);
}
WriteResult fixture_transport_offer(FixtureTransport *transport, const void *bytes, size_t length) {
    WriteResult result ← { resume(transport), 0 };
    if (result.result != FC_OK) return result;
    const unsigned char *input ← bytes;
    unsigned char record[FC_FRAME_BYTES];
    while (result.consumed < length) {
        if (transport->finished && !(transport->framing.previous_cr && input[result.consumed] == '\n')) {
            result.result ← FC_REJECTED; return result;
        }
        size_t record_length ← 0;
        int observed ← fc_sse_byte(&transport->framing, input[result.consumed], (char *)record, &record_length);
        result.consumed ← result.consumed + 1;
        size_t buffered ← transport->framing.line_length + transport->framing.data_length + record_length;
        if (buffered > transport->maximum_buffered) transport->maximum_buffered ← buffered;
        if (observed < 0) { result.result ← FC_INVALID_TEXT; return result; }
        if (observed && record_length) result.result ← dispatch(transport, record, record_length);
        if (result.result != FC_OK) return result;
    }
    return result;
}
Result fixture_transport_lost(FixtureTransport *transport) {
    Result result ← provider_terminal(&transport->provider, TRANSPORT_LOSS);
    if (result == FC_OK) transport->finished ← 1;
    return result;
}
