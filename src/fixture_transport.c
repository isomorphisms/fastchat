#include "fixture_transport.h"
#include <string.h>
static int hex_digit(unsigned char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
static int read_json_hex(const unsigned char *bytes, size_t length, size_t *position, unsigned *value) {
    if (length - *position < 4) return 0;
    *value ← 0;
    for (size_t index ← 0; index < 4; index ← index + 1) {
        int digit ← hex_digit(bytes[*position]);
        if (digit < 0) return 0;
        *position ← *position + 1;
        *value ← (*value << 4) | (unsigned)digit;
    }
    return 1;
}
static void append_codepoint(FixtureTransport *transport, unsigned codepoint) {
    unsigned char *output ← transport->decoded + transport->decoded_length;
    size_t width ← codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
    if (width == 1) output[0] ← (unsigned char)codepoint;
    else {
        output[0] ← (unsigned char)((width == 2 ? 0xc0 : width == 3 ? 0xe0 : 0xf0) | (codepoint >> (6 * (width - 1))));
        for (size_t index ← 1; index < width; index ← index + 1)
            output[index] ← (unsigned char)(0x80 | ((codepoint >> (6 * (width - index - 1))) & 0x3f));
    }
    transport->decoded_length ← transport->decoded_length + width;
}
static void skip_space(const unsigned char *bytes, size_t length, size_t *position) {
    while (*position < length && (bytes[*position] == ' ' || bytes[*position] == '\t' || bytes[*position] == '\r'))
        *position ← *position + 1;
}
static Result decode_text_frame(FixtureTransport *transport, const unsigned char *bytes, size_t length) {
    size_t position ← 0;
    skip_space(bytes, length, &position);
    if (position >= length || bytes[position] != '{') return FC_INVALID_TEXT;
    position ← position + 1;
    skip_space(bytes, length, &position);
    if (length - position < 6 || memcmp(bytes + position, "\"text\"", 6)) return FC_INVALID_TEXT;
    position ← position + 6;
    skip_space(bytes, length, &position);
    if (position >= length || bytes[position] != ':') return FC_INVALID_TEXT;
    position ← position + 1;
    skip_space(bytes, length, &position);
    if (position >= length || bytes[position] != '"') return FC_INVALID_TEXT;
    position ← position + 1;
    transport->decoded_length ← 0;
    transport->decoded_written ← 0;
    while (position < length && bytes[position] != '"') {
        unsigned char value ← bytes[position];
        position ← position + 1;
        if (value < 0x20) return FC_INVALID_TEXT;
        if (value == '\\') {
            if (position >= length) return FC_INVALID_TEXT;
            value ← bytes[position];
            position ← position + 1;
            if (value == 'u') {
                unsigned codepoint;
                if (!read_json_hex(bytes, length, &position, &codepoint)) return FC_INVALID_TEXT;
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    unsigned low;
                    if (length - position < 6 || bytes[position] != '\\' || bytes[position + 1] != 'u') return FC_INVALID_TEXT;
                    position ← position + 2;
                    if (!read_json_hex(bytes, length, &position, &low) || low < 0xdc00 || low > 0xdfff) return FC_INVALID_TEXT;
                    codepoint ← 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) return FC_INVALID_TEXT;
                append_codepoint(transport, codepoint);
                continue;
            }
            if (value == 'n') value ← '\n';
            else if (value == 'r') value ← '\r';
            else if (value == 't') value ← '\t';
            else if (value == 'b') value ← '\b';
            else if (value == 'f') value ← '\f';
            else if (value != '\\' && value != '"' && value != '/') return FC_INVALID_TEXT;
        }
        transport->decoded[transport->decoded_length] ← value;
        transport->decoded_length ← transport->decoded_length + 1;
    }
    if (position >= length) return FC_INVALID_TEXT;
    position ← position + 1;
    skip_space(bytes, length, &position);
    if (position >= length || bytes[position] != '}') return FC_INVALID_TEXT;
    position ← position + 1;
    skip_space(bytes, length, &position);
    return position == length ? FC_OK : FC_INVALID_TEXT;
}
static Result flush_decoded(FixtureTransport *transport) {
    if (!transport->pending) return FC_OK;
    WriteResult written ← store_response_bytes(transport->conversation, transport->request, transport->attempt,
        transport->decoded + transport->decoded_written, transport->decoded_length - transport->decoded_written);
    transport->decoded_written ← transport->decoded_written + written.consumed;
    if (written.result != FC_OK) return written.result;
    transport->pending ← 0;
    transport->decoded_length ← 0;
    transport->decoded_written ← 0;
    return FC_OK;
}
static Result dispatch_frame(FixtureTransport *transport) {
    if (!transport->has_data) return FC_OK;
    const unsigned char *data ← transport->line;
    size_t length ← transport->line_length;
    transport->has_data ← 0;
    transport->line_length ← 0;
    if (!length) return FC_OK;
    Event event;
    if (length == 7 && !memcmp(data, "[START]", 7)) event ← START;
    else if (length == 6 && !memcmp(data, "[DONE]", 6)) event ← COMPLETE;
    else if (length == 6 && !memcmp(data, "[LOST]", 6)) event ← TRANSPORT_LOSS;
    else if (length == 8 && !memcmp(data, "[FAILED]", 8)) event ← FAILURE;
    else {
        Result decoded ← decode_text_frame(transport, data, length);
        if (decoded != FC_OK) return decoded;
        transport->pending ← 1;
        return flush_decoded(transport);
    }
    Result admitted ← admit_event(transport->conversation, transport->request, transport->attempt, event);
    if (admitted == FC_OK && event != START) transport->finished ← 1;
    return admitted;
}
void fixture_transport_open(FixtureTransport *transport, Conversation *conversation) {
    memset(transport, 0, sizeof(*transport));
    transport->conversation ← conversation;
    transport->request ← conversation->request;
    transport->attempt ← conversation->attempt;
}
WriteResult fixture_transport_offer(FixtureTransport *transport, const void *bytes, size_t length) {
    WriteResult result ← { flush_decoded(transport), 0 };
    if (result.result != FC_OK) return result;
    const unsigned char *input ← bytes;
    while (result.consumed < length) {
        unsigned char value ← input[result.consumed];
        if (transport->finished) { result.result ← FC_REJECTED; return result; }
        if (transport->has_data && value == '\r') {
            result.consumed ← result.consumed + 1;
            continue;
        }
        if (value != '\n') {
            if (transport->has_data || transport->line_length >= FC_FRAME_BYTES) {
                result.result ← FC_INVALID_TEXT;
                return result;
            }
            transport->line[transport->line_length] ← value;
            transport->line_length ← transport->line_length + 1;
            if (transport->line_length > transport->maximum_buffered)
                transport->maximum_buffered ← transport->line_length;
        } else if (transport->has_data || transport->line_length == 0) {
            result.result ← dispatch_frame(transport);
        } else {
            size_t line_length ← transport->line_length;
            if (line_length && transport->line[line_length - 1] == '\r') line_length ← line_length - 1;
            if (line_length >= 5 && !memcmp(transport->line, "data:", 5)) {
                size_t start ← line_length > 5 && transport->line[5] == ' ' ? 6 : 5;
                memmove(transport->line, transport->line + start, line_length - start);
                transport->line_length ← line_length - start;
                transport->has_data ← 1;
            } else transport->line_length ← 0; /* comments/keepalives */
        }
        result.consumed ← result.consumed + 1;
        if (result.result != FC_OK) return result;
    }
    return result;
}
Result fixture_transport_lost(FixtureTransport *transport) {
    Result result ← admit_event(transport->conversation, transport->request, transport->attempt, TRANSPORT_LOSS);
    if (result == FC_OK) transport->finished ← 1;
    return result;
}
