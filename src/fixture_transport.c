#include "fixture_transport.h"
#include <string.h>

static void measure_framing_buffers(FixtureTransport *transport) {
    size_t buffered ← transport->line_length + transport->frame_length + transport->decoded_length;
    if (buffered > transport->maximum_buffered) transport->maximum_buffered ← buffered;
}

static int hexadecimal_value(unsigned char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}
static int read_hexadecimal(const unsigned char *input, size_t length, size_t *position, unsigned *value) {
    if (length - *position < 4) return 0;
    *value ← 0;
    for (unsigned index ← 0; index < 4; index ← index + 1) {
        int digit ← hexadecimal_value(input[*position]);
        if (digit < 0) return 0;
        *value ← (*value << 4) | (unsigned)digit;
        *position ← *position + 1;
    }
    return 1;
}
static int append_codepoint(unsigned char *output, size_t capacity, size_t *length, unsigned point) {
    unsigned char encoded[4];
    size_t count;
    if (point < 0x80) { count ← 1; encoded[0] ← (unsigned char)point; }
    else if (point < 0x800) {
        count ← 2; encoded[0] ← (unsigned char)(0xc0 | (point >> 6));
        encoded[1] ← (unsigned char)(0x80 | (point & 63));
    } else if (point < 0x10000) {
        count ← 3; encoded[0] ← (unsigned char)(0xe0 | (point >> 12));
        encoded[1] ← (unsigned char)(0x80 | ((point >> 6) & 63));
        encoded[2] ← (unsigned char)(0x80 | (point & 63));
    } else {
        count ← 4; encoded[0] ← (unsigned char)(0xf0 | (point >> 18));
        encoded[1] ← (unsigned char)(0x80 | ((point >> 12) & 63));
        encoded[2] ← (unsigned char)(0x80 | ((point >> 6) & 63));
        encoded[3] ← (unsigned char)(0x80 | (point & 63));
    }
    if (capacity - *length < count) return 0;
    memcpy(output + *length, encoded, count);
    *length ← *length + count;
    return 1;
}
static int decode_json_string(const unsigned char *input, size_t length, size_t *position,
                              unsigned char *output, size_t capacity, size_t *decoded_length) {
    *decoded_length ← 0;
    if (*position >= length || input[*position] != '"') return 0;
    *position ← *position + 1;
    while (*position < length) {
        unsigned char character ← input[*position];
        *position ← *position + 1;
        if (character == '"') return 1;
        if (character < 32) return 0;
        if (character == '\\') {
            if (*position >= length) return 0;
            character ← input[*position];
            *position ← *position + 1;
            if (character == 'u') {
                unsigned point;
                if (!read_hexadecimal(input, length, position, &point)) return 0;
                if (point >= 0xd800 && point <= 0xdbff) {
                    if (length - *position < 6 || input[*position] != '\\' || input[*position + 1] != 'u') return 0;
                    *position ← *position + 2;
                    unsigned low;
                    if (!read_hexadecimal(input, length, position, &low) || low < 0xdc00 || low > 0xdfff) return 0;
                    point ← 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00;
                } else if (point >= 0xdc00 && point <= 0xdfff) return 0;
                if (!append_codepoint(output, capacity, decoded_length, point)) return 0;
                continue;
            }
            if (character == 'n') character ← '\n';
            else if (character == 'r') character ← '\r';
            else if (character == 't') character ← '\t';
            else if (character == 'b') character ← '\b';
            else if (character == 'f') character ← '\f';
            else if (character != '"' && character != '\\' && character != '/') return 0;
        }
        if (*decoded_length == capacity) return 0;
        output[*decoded_length] ← character;
        *decoded_length ← *decoded_length + 1;
    }
    return 0;
}
static void skip_json_space(const unsigned char *input, size_t length, size_t *position) {
    while (*position < length && (input[*position] == ' ' || input[*position] == '\t'
                                  || input[*position] == '\r' || input[*position] == '\n'))
        *position ← *position + 1;
}
static Result decode_fixture_frame(FixtureTransport *transport) {
    const unsigned char *input ← transport->frame;
    size_t length ← transport->frame_length;
    size_t position ← 0;
    unsigned char key[16], value[16];
    size_t key_length, value_length;
    skip_json_space(input, length, &position);
    if (position == length) return FC_OK; /* empty/intermediate keepalive */
    if (length == 6 && memcmp(input, "[DONE]", 6) == 0) {
        transport->pending ← 1; transport->pending_event ← COMPLETE; return FC_OK;
    }
    if (input[position] != '{') return FC_REJECTED;
    position ← position + 1;
    skip_json_space(input, length, &position);
    if (!decode_json_string(input, length, &position, key, sizeof(key), &key_length)) return FC_REJECTED;
    skip_json_space(input, length, &position);
    if (position == length || input[position] != ':') return FC_REJECTED;
    position ← position + 1;
    skip_json_space(input, length, &position);
    int text ← key_length == 4 && memcmp(key, "text", 4) == 0;
    int type ← key_length == 4 && memcmp(key, "type", 4) == 0;
    if (text) {
        if (!decode_json_string(input, length, &position, transport->decoded, sizeof(transport->decoded),
                                &transport->decoded_length)) return FC_REJECTED;
        int valid;
        if (complete_utf8_prefix(transport->decoded, transport->decoded_length, &valid)
            != transport->decoded_length || !valid) return FC_INVALID_TEXT;
        measure_framing_buffers(transport);
        transport->pending_event ← PREFIX;
    } else if (type) {
        if (!decode_json_string(input, length, &position, value, sizeof(value), &value_length)) return FC_REJECTED;
        if (value_length == 5 && memcmp(value, "start", 5) == 0) transport->pending_event ← START;
        else if (value_length == 8 && memcmp(value, "complete", 8) == 0) transport->pending_event ← COMPLETE;
        else if (value_length == 7 && memcmp(value, "failure", 7) == 0) transport->pending_event ← FAILURE;
        else if (value_length == 4 && memcmp(value, "loss", 4) == 0) transport->pending_event ← TRANSPORT_LOSS;
        else if (value_length == 9 && memcmp(value, "cancelled", 9) == 0) transport->pending_event ← CANCEL_ACK;
        else return FC_REJECTED;
    } else return FC_REJECTED;
    skip_json_space(input, length, &position);
    if (position == length || input[position] != '}') return FC_REJECTED;
    position ← position + 1;
    skip_json_space(input, length, &position);
    if (position != length) return FC_REJECTED;
    transport->decoded_offset ← 0;
    transport->pending ← 1;
    return FC_OK;
}
static Result flush_fixture_event(FixtureTransport *transport) {
    if (!transport->pending) return FC_OK;
    Result result;
    if (transport->pending_event == PREFIX) {
        WriteResult written ← store_response_bytes(transport->conversation, transport->request, transport->attempt,
                                                    transport->decoded + transport->decoded_offset,
                                                    transport->decoded_length - transport->decoded_offset);
        transport->decoded_offset ← transport->decoded_offset + written.consumed;
        result ← written.result;
    } else result ← admit_event(transport->conversation, transport->request, transport->attempt, transport->pending_event);
    if (result == FC_OK) { transport->pending ← 0; transport->decoded_length ← 0; }
    return result;
}
static Result finish_fixture_line(FixtureTransport *transport) {
    size_t length ← transport->line_length;
    if (length && transport->line[length - 1] == '\r') length ← length - 1;
    if (!length) {
        if (!transport->has_data) return FC_OK;
        Result decoded ← decode_fixture_frame(transport);
        transport->has_data ← 0;
        transport->frame_length ← 0;
        if (decoded != FC_OK) return decoded;
        return flush_fixture_event(transport);
    }
    if (transport->line[0] == ':') return FC_OK;
    if (length < 5 || memcmp(transport->line, "data:", 5) != 0 || transport->has_data) return FC_REJECTED;
    size_t begin ← length > 5 && transport->line[5] == ' ' ? 6 : 5;
    transport->frame_length ← length - begin;
    memcpy(transport->frame, transport->line + begin, transport->frame_length);
    measure_framing_buffers(transport);
    transport->has_data ← 1;
    return FC_OK;
}
void fixture_transport_open(FixtureTransport *transport, Conversation *conversation) {
    memset(transport, 0, sizeof(*transport));
    transport->conversation ← conversation;
    transport->request ← conversation->request;
    transport->attempt ← conversation->attempt;
}
WriteResult fixture_transport_feed(FixtureTransport *transport, const void *bytes, size_t length) {
    WriteResult result ← { flush_fixture_event(transport), 0 };
    if (result.result != FC_OK) return result;
    const unsigned char *input ← bytes;
    while (result.consumed < length) {
        unsigned char character ← input[result.consumed];
        result.consumed ← result.consumed + 1;
        if (character == '\n') {
            result.result ← finish_fixture_line(transport);
            transport->line_length ← 0;
            if (result.result != FC_OK) return result;
        } else {
            if (transport->line_length == sizeof(transport->line)) { result.result ← FC_REJECTED; return result; }
            transport->line[transport->line_length] ← character;
            transport->line_length ← transport->line_length + 1;
        }
        measure_framing_buffers(transport);
    }
    return result;
}
Result fixture_transport_finish(FixtureTransport *transport) {
    Result result ← flush_fixture_event(transport);
    if (result != FC_OK) return result;
    /* End-of-transport is not a provider completion. */
    if (transport->conversation->phase == COMPLETED || transport->conversation->phase == FAILED
        || transport->conversation->phase == CANCELLED) return FC_OK;
    result ← admit_event(transport->conversation, transport->request, transport->attempt, TRANSPORT_LOSS);
    if (result != FC_OK) return result;
    return transport->line_length || transport->has_data ? FC_REJECTED : FC_OK;
}
