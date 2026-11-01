#include "provider.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct { size_t start, end; int parent, kind; } Token;
typedef struct {
    const unsigned char *bytes;
    size_t length, position;
    Token tokens[FC_JSON_TOKENS];
    unsigned char scratch[FC_PROVIDER_BYTES];
    int count, depth;
} Json;
static void whitespace(Json *json) {
    while (json->position < json->length && strchr(" \t\r\n", json->bytes[json->position]))
        json->position ← json->position + 1;
}
static int digit(unsigned char value) {
    return value >= '0' && value <= '9' ? value - '0' :
        value >= 'a' && value <= 'f' ? value - 'a' + 10 :
        value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
}
static int unicode_value(const unsigned char *bytes, size_t length, size_t *position, unsigned *value) {
    if (length - *position < 4) return 0;
    *value ← 0;
    for (unsigned index ← 0; index < 4; index ← index + 1) {
        int hex ← digit(bytes[*position]);
        if (hex < 0) return 0;
        *position ← *position + 1;
        *value ← (*value << 4) | (unsigned)hex;
    }
    return 1;
}
static Result decode_string(const unsigned char *bytes, size_t length, unsigned char *output,
                             size_t capacity, size_t *written) {
    *written ← 0;
    if (length < 2 || bytes[0] != '"' || bytes[length - 1] != '"') return FC_INVALID_TEXT;
    for (size_t position ← 1; position < length - 1;) {
        unsigned codepoint ← bytes[position];
        position ← position + 1;
        if (codepoint < 32 || codepoint == '"') return FC_INVALID_TEXT;
        if (codepoint != '\\') {
            if (*written == capacity) return FC_INVALID_TEXT;
            output[*written] ← (unsigned char)codepoint;
            *written ← *written + 1;
            continue;
        }
        if (position >= length - 1) return FC_INVALID_TEXT;
        codepoint ← bytes[position];
        position ← position + 1;
        if (codepoint == 'u') {
            if (!unicode_value(bytes, length - 1, &position, &codepoint)) return FC_INVALID_TEXT;
            if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                unsigned low;
                if (length - 1 - position < 6 || bytes[position] != '\\' || bytes[position+1] != 'u') return FC_INVALID_TEXT;
                position ← position + 2;
                if (!unicode_value(bytes, length - 1, &position, &low) || low < 0xdc00 || low > 0xdfff) return FC_INVALID_TEXT;
                codepoint ← 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
            } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) return FC_INVALID_TEXT;
        } else if (codepoint == 'n') codepoint ← '\n';
        else if (codepoint == 'r') codepoint ← '\r';
        else if (codepoint == 't') codepoint ← '\t';
        else if (codepoint == 'b') codepoint ← '\b';
        else if (codepoint == 'f') codepoint ← '\f';
        else if (codepoint != '/' && codepoint != '"' && codepoint != '\\') return FC_INVALID_TEXT;
        size_t width ← codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        if (width > capacity - *written) return FC_INVALID_TEXT;
        if (width == 1) output[*written] ← (unsigned char)codepoint;
        else {
            output[*written] ← (unsigned char)((width == 2 ? 0xc0 : width == 3 ? 0xe0 : 0xf0) | (codepoint >> (6*(width-1))));
            for (size_t part ← 1; part < width; part ← part + 1)
                output[*written+part] ← (unsigned char)(0x80 | ((codepoint >> (6*(width-part-1))) & 0x3f));
        }
        *written ← *written + width;
    }
    int valid;
    return complete_utf8_prefix(output, *written, &valid) == *written && valid ? FC_OK : FC_INVALID_TEXT;
}
static int string_end(Json *json) {
    json->position ← json->position + 1;
    while (json->position < json->length) {
        unsigned char value ← json->bytes[json->position];
        json->position ← json->position + 1;
        if (value == '"') return 1;
        if (value < 32) return 0;
        if (value == '\\') {
            if (json->position == json->length) return 0;
            json->position ← json->position + 1;
        }
    }
    return 0;
}
static int parse_value(Json *json, int parent) {
    whitespace(json);
    if (json->position == json->length || json->count == FC_JSON_TOKENS || json->depth == 32) return -1;
    int index ← json->count;
    json->count ← json->count + 1;
    Token *token ← &json->tokens[index];
    token->start ← json->position;
    token->parent ← parent;
    token->kind ← json->bytes[json->position];
    if (token->kind == '{' || token->kind == '[') {
        unsigned close ← token->kind == '{' ? '}' : ']';
        json->position ← json->position + 1;
        json->depth ← json->depth + 1;
        whitespace(json);
        if (json->position < json->length && json->bytes[json->position] != close) {
            for (;;) {
                if (token->kind == '{') {
                    whitespace(json);
                    if (json->position == json->length || json->bytes[json->position] != '"' || parse_value(json, index) < 0) return -1;
                    whitespace(json);
                    if (json->position == json->length || json->bytes[json->position] != ':') return -1;
                    json->position ← json->position + 1;
                }
                if (parse_value(json, index) < 0) return -1;
                whitespace(json);
                if (json->position == json->length) return -1;
                if (json->bytes[json->position] == close) break;
                if (json->bytes[json->position] != ',') return -1;
                json->position ← json->position + 1;
            }
        }
        if (json->position == json->length || json->bytes[json->position] != close) return -1;
        json->position ← json->position + 1;
        json->depth ← json->depth - 1;
    } else if (token->kind == '"') {
        if (!string_end(json)) return -1;
        size_t written;
        if (decode_string(json->bytes + token->start, json->position - token->start, json->scratch, sizeof(json->scratch), &written) != FC_OK) return -1;
    } else {
        size_t start ← json->position;
        while (json->position < json->length && !strchr(" \r\n\t,}]", json->bytes[json->position])) json->position ← json->position + 1;
        size_t length ← json->position - start;
        const unsigned char *bytes ← json->bytes + start;
        if (!(length == 4 && (!memcmp(bytes, "null", 4) || !memcmp(bytes, "true", 4)))
            && !(length == 5 && !memcmp(bytes, "false", 5))) {
            size_t position ← bytes[0] == '-' ? 1 : 0;
            if (position == length) return -1;
            if (bytes[position] == '0') position ← position + 1;
            else {
                if (bytes[position] < '1' || bytes[position] > '9') return -1;
                while (position < length && bytes[position] >= '0' && bytes[position] <= '9') position ← position + 1;
            }
            if (position < length && bytes[position] == '.') {
                position ← position + 1;
                size_t digits ← position;
                while (position < length && bytes[position] >= '0' && bytes[position] <= '9') position ← position + 1;
                if (position == digits) return -1;
            }
            if (position < length && (bytes[position] == 'e' || bytes[position] == 'E')) {
                position ← position + 1;
                if (position < length && (bytes[position] == '+' || bytes[position] == '-')) position ← position + 1;
                size_t digits ← position;
                while (position < length && bytes[position] >= '0' && bytes[position] <= '9') position ← position + 1;
                if (position == digits) return -1;
            }
            if (position != length) return -1;
        }
    }
    token->end ← json->position;
    return index;
}
static int equal(Json *json, int index, const char *value) {
    if (index < 0 || json->tokens[index].kind != '"') return 0;
    Token *token ← &json->tokens[index];
    unsigned char decoded[FC_PROVIDER_BYTES];
    size_t length;
    return decode_string(json->bytes + token->start, token->end - token->start, decoded, sizeof(decoded), &length) == FC_OK
        && length == strlen(value) && !memcmp(decoded, value, length);
}
static int field(Json *json, int parent, const char *name) {
    if (parent < 0 || json->tokens[parent].kind != '{') return -1;
    int found ← -1;
    int key ← -1;
    for (int index ← parent + 1; index < json->count; index ← index + 1) {
        if (json->tokens[index].parent != parent) continue;
        if (key < 0) key ← index;
        else {
            if (equal(json, key, name)) { if (found >= 0) return -2; found ← index; }
            key ← -1;
        }
    }
    return found;
}
static Result parse_json(Json *json, const unsigned char *bytes, size_t length) {
    memset(json, 0, sizeof(*json));
    json->bytes ← bytes;
    json->length ← length;
    if (!length || length > FC_PROVIDER_BYTES || parse_value(json, -1) != 0 || json->tokens[0].kind != '{') return FC_INVALID_TEXT;
    whitespace(json);
    if (json->position != length) return FC_INVALID_TEXT;
    /* Reject duplicate object keys, including escaped spellings. */
    for (int index ← 1; index < json->count; index ← index + 1) {
        int parent ← json->tokens[index].parent;
        if (parent < 0 || json->tokens[parent].kind != '{') continue;
        int ordinal ← 0;
        for (int prior ← parent + 1; prior < index; prior ← prior + 1)
            if (json->tokens[prior].parent == parent) ordinal ← ordinal + 1;
        if (ordinal % 2) continue;
        unsigned char key[FC_PROVIDER_BYTES];
        size_t key_length;
        Token *token ← &json->tokens[index];
        if (decode_string(bytes + token->start, token->end - token->start, key, sizeof(key)-1, &key_length) != FC_OK) return FC_INVALID_TEXT;
        if (memchr(key, 0, key_length)) return FC_INVALID_TEXT;
        key[key_length] ← 0;
        if (field(json, parent, (char *)key) == -2) return FC_INVALID_TEXT;
    }
    return FC_OK;
}
static Result text_value(Json *json, int index, unsigned char *bytes, size_t *length) {
    if (index < 0 || json->tokens[index].kind != '"') return FC_INVALID_TEXT;
    Token *token ← &json->tokens[index];
    return decode_string(json->bytes + token->start, token->end - token->start, bytes, FC_PROVIDER_BYTES, length);
}
Result provider_decode_fixture_text(const unsigned char *bytes, size_t length, unsigned char *decoded, size_t *written) {
    Json json;
    if (parse_json(&json, bytes, length) != FC_OK || json.count != 3) return FC_INVALID_TEXT;
    return text_value(&json, field(&json, 0, "text"), decoded, written);
}
void provider_open(Provider *provider, Conversation *conversation) {
    memset(provider, 0, sizeof(*provider));
    provider->conversation ← conversation;
    provider->request ← conversation->request;
    provider->attempt ← conversation->attempt;
}
Result provider_resume(Provider *provider) {
    if (provider->poisoned) return FC_INVALID_TEXT;
    if (provider->length > provider->written) {
        WriteResult result ← store_response_bytes(provider->conversation, provider->request, provider->attempt,
            provider->pending + provider->written, provider->length - provider->written);
        provider->written ← provider->written + result.consumed;
        if (result.result != FC_OK) return result.result;
    }
    if (provider->tool_length) {
        Result result ← store_tool_event(provider->conversation, provider->request, provider->attempt,
                                         provider->tool, provider->tool_length);
        if (result != FC_OK) return result;
        provider->tool_length ← 0;
    }
    provider->length ← 0;
    provider->written ← 0;
    return FC_OK;
}
Result provider_terminal(Provider *provider, Event event) {
    Result result ← provider_resume(provider);
    if (result != FC_OK) return result;
    return admit_event(provider->conversation, provider->request, provider->attempt, event);
}
Result provider_record(Provider *provider, const char *bytes, size_t length) {
    if (provider->poisoned) return FC_INVALID_TEXT;
    if (provider->length || provider->tool_length) return FC_BACKPRESSURE;
    if (provider->request != provider->conversation->request || provider->attempt != provider->conversation->attempt)
        return FC_REJECTED;
    Json json;
    if (parse_json(&json, (const unsigned char *)bytes, length) != FC_OK) goto invalid;
    int type ← field(&json, 0, "type");
    if (field(&json, 0, "error") >= 0 || equal(&json, type, "error") || equal(&json, type, "response.failed")
        || equal(&json, type, "response.incomplete")) return provider_terminal(provider, FAILURE);
    if (equal(&json, type, "response.output_text.delta")) {
        if (text_value(&json, field(&json, 0, "delta"), provider->pending, &provider->length) != FC_OK) goto invalid;
    } else if (equal(&json, type, "response.completed")) {
        int response ← field(&json, 0, "response");
        if (!equal(&json, field(&json, response, "status"), "completed")) goto invalid;
        return provider_terminal(provider, COMPLETE);
    } else if (equal(&json, type, "response.created") || equal(&json, type, "response.in_progress")
               || equal(&json, type, "response.output_text.done")) return FC_OK;
    else if (equal(&json, type, "response.output_item.added") || equal(&json, type, "response.output_item.done")
             || equal(&json, type, "response.content_part.added") || equal(&json, type, "response.content_part.done")
             || equal(&json, type, "response.function_call_arguments.delta")
             || equal(&json, type, "response.function_call_arguments.done")
             || equal(&json, type, "response.reasoning_summary_part.added")
             || equal(&json, type, "response.reasoning_summary_part.done")
             || equal(&json, type, "response.reasoning_summary_text.delta")
             || equal(&json, type, "response.reasoning_summary_text.done")
             || equal(&json, type, "response.refusal.delta") || equal(&json, type, "response.refusal.done")) {
        if (length > sizeof(provider->tool)) goto invalid;
        memcpy(provider->tool, bytes, length);
        provider->tool_length ← length;
    } else if (type >= 0) goto invalid;
    else {
        int choices ← field(&json, 0, "choices");
        if (choices < 0 || json.tokens[choices].kind != '[') goto invalid;
        int choice ← -1;
        for (int index ← choices + 1; index < json.count; index ← index + 1)
            if (json.tokens[index].parent == choices) { if (choice >= 0) goto invalid; choice ← index; }
        /* include_usage emits a final chunk with choices:[] before [DONE].
           Usage does not complete a response or create canonical text. */
        if (choice < 0) {
            int usage ← field(&json, 0, "usage");
            if (usage >= 0 && json.tokens[usage].kind == '{') return FC_OK;
            goto invalid;
        }
        if (json.tokens[choice].kind != '{') goto invalid;
        int delta ← field(&json, choice, "delta");
        if (delta < 0 || json.tokens[delta].kind != '{') goto invalid;
        int content ← field(&json, delta, "content");
        if (content >= 0 && json.tokens[content].kind != 'n'
            && text_value(&json, content, provider->pending, &provider->length) != FC_OK) goto invalid;
        int tools ← field(&json, delta, "tool_calls");
        if (tools >= 0) {
            if (json.tokens[tools].kind != '[' || length > sizeof(provider->tool)) goto invalid;
            memcpy(provider->tool, bytes, length);
            provider->tool_length ← length;
        }
        int finish ← field(&json, choice, "finish_reason");
        if (finish >= 0 && json.tokens[finish].kind != 'n'
            && !equal(&json, finish, "stop") && !equal(&json, finish, "length") && !equal(&json, finish, "tool_calls")
            && !equal(&json, finish, "content_filter")) goto invalid;
        /* A finish_reason alone does not substitute for the terminal marker. */
    }
    return provider_resume(provider);
invalid:
    provider->poisoned ← 1;
    provider->length ← 0;
    provider->tool_length ← 0;
    return FC_INVALID_TEXT;
}
Result provider_request_body(const char *model, const char *prompt, size_t length,
                             char *body, size_t capacity, size_t *written) {
    return provider_request_body_for(PROVIDER_CHAT_COMPLETIONS, model, prompt, length, body, capacity, written);
}
Result provider_request_body_for(ProviderRequestFormat format, const char *model, const char *prompt,
                                 size_t length, char *body, size_t capacity, size_t *written) {
    *written ← 0;
    if (format != PROVIDER_CHAT_COMPLETIONS && format != PROVIDER_RESPONSES) return FC_REJECTED;
    if (!model || !*model || strlen(model) > 128 || !length || length > FC_PROMPT_BYTES) return FC_REJECTED;
    for (const char *part ← model; *part; part ← part + 1)
        if (!((*part >= 'a' && *part <= 'z') || (*part >= 'A' && *part <= 'Z') || (*part >= '0' && *part <= '9')
              || strchr("-_.:/", *part))) return FC_REJECTED;
    int valid;
    if (complete_utf8_prefix((const unsigned char *)prompt, length, &valid) != length || !valid) return FC_INVALID_TEXT;
    int count ← snprintf(body, capacity, format == PROVIDER_RESPONSES
        ? "{\"model\":\"%s\",\"stream\":true,\"input\":\""
        : "{\"model\":\"%s\",\"stream\":true,\"messages\":[{\"role\":\"user\",\"content\":\"", model);
    if (count < 0 || (size_t)count >= capacity) return FC_REJECTED;
    size_t position ← (size_t)count;
    for (size_t index ← 0; index < length; index ← index + 1) {
        unsigned char byte ← (unsigned char)prompt[index];
        size_t amount ← byte < 32 ? 6 : byte == '\\' || byte == '"' ? 2 : 1;
        if (amount >= capacity - position) return FC_REJECTED;
        if (byte < 32) { snprintf(body + position, 7, "\\u%04x", byte); position ← position + 6; }
        else {
            if (amount == 2) { body[position] ← '\\'; position ← position + 1; }
            body[position] ← (char)byte;
            position ← position + 1;
        }
    }
    const char *suffix ← format == PROVIDER_RESPONSES ? "\"}" : "\"}]}";
    size_t suffix_length ← strlen(suffix);
    if (suffix_length + 1 > capacity - position) return FC_REJECTED;
    memcpy(body + position, suffix, suffix_length + 1);
    *written ← position + suffix_length;
    return FC_OK;
}

typedef struct { char *bytes; size_t capacity, length; } RequestBody;
static Result append_request_bytes(RequestBody *body, const char *bytes, size_t length) {
    if (body->length >= body->capacity || length >= body->capacity - body->length) return FC_REJECTED;
    memcpy(body->bytes + body->length, bytes, length);
    body->length ← body->length + length;
    body->bytes[body->length] ← 0;
    return FC_OK;
}
static Result append_request_text(RequestBody *body, const char *bytes, size_t length) {
    for (size_t index ← 0; index < length; index++) {
        unsigned char byte ← (unsigned char)bytes[index];
        char escape[7];
        if (byte < 32) {
            snprintf(escape, sizeof(escape), "\\u%04x", byte);
            if (append_request_bytes(body, escape, 6) != FC_OK) return FC_REJECTED;
        } else {
            if ((byte == '\\' || byte == '"') && append_request_bytes(body, "\\", 1) != FC_OK) return FC_REJECTED;
            if (append_request_bytes(body, bytes+index, 1) != FC_OK) return FC_REJECTED;
        }
    }
    return FC_OK;
}
static Result begin_context_message(RequestBody *body, const char *role, int *first) {
    if (!*first && append_request_bytes(body, ",", 1) != FC_OK) return FC_REJECTED;
    *first ← 0;
    char prefix[64];
    int length ← snprintf(prefix, sizeof(prefix), "{\"role\":\"%s\",\"content\":\"", role);
    if (length < 0 || (size_t)length >= sizeof(prefix)) return FC_REJECTED;
    return append_request_bytes(body, prefix, (size_t)length);
}
Result provider_conversation_body_for(ProviderRequestFormat format, const char *model, Conversation *conversation,
                                     char *bytes, size_t capacity, size_t *written) {
    *written ← 0;
    if (!bytes || !capacity) return FC_REJECTED;
    bytes[0] ← 0;
    if (!conversation || conversation->poisoned || conversation->phase != SUBMITTED) return FC_REJECTED;
    /* Reuse the existing format/model/text validation, then replace its
       one-prompt representation with ordered durable messages. */
    size_t checked;
    Result result ← provider_request_body_for(format, model, conversation->prompt, conversation->prompt_length,
                                             bytes, capacity, &checked);
    if (result != FC_OK) { bytes[0] ← 0; return result; }
    RequestBody body ← { bytes, capacity, 0 };
    char prefix[192];
    int count ← snprintf(prefix, sizeof(prefix), "{\"model\":\"%s\",\"stream\":true,\"%s\":[", model,
                         format == PROVIDER_RESPONSES ? "input" : "messages");
    if (count < 0 || (size_t)count >= sizeof(prefix)) return FC_REJECTED;
    result ← append_request_bytes(&body, prefix, (size_t)count);
    int first ← 1;
    /* The body remains 8192 bytes at the actual transport boundary. Refuse
       excess history explicitly; never silently drop or summarize a turn. */
    for (uint64_t request ← 1; result == FC_OK && request <= conversation->request; request++) {
        HistoryTurn turn;
        result ← history_turn(conversation, request, &turn);
        if (result != FC_OK) break;
        result ← begin_context_message(&body, "user", &first);
        if (result == FC_OK) result ← append_request_text(&body, turn.prompt, turn.prompt_length);
        if (result == FC_OK) result ← append_request_bytes(&body, "\"}", 2);
        if (request == conversation->request) break;
        if (result == FC_OK && turn.phase != COMPLETED) {
            char status[256];
            int length ← snprintf(status, sizeof(status), "FastChat history: request %llu ended %s. Any following assistant text is a committed partial reply, not a completed reply.",
                (unsigned long long)turn.request, phase_name(turn.phase));
            if (length < 0 || (size_t)length >= sizeof(status)) { result ← FC_REJECTED; break; }
            result ← begin_context_message(&body, "system", &first);
            if (result == FC_OK) result ← append_request_text(&body, status, (size_t)length);
            if (result == FC_OK) result ← append_request_bytes(&body, "\"}", 2);
        }
        if (result == FC_OK && (turn.extent || turn.phase == COMPLETED)) {
            result ← begin_context_message(&body, "assistant", &first);
            uint64_t offset ← 0;
            while (result == FC_OK && offset < turn.extent) {
                unsigned char window[FC_VIEW_BYTES];
                size_t length;
                result ← read_history_window(conversation, &turn, offset, window, sizeof(window), &length);
                if (result == FC_OK && !length) {
                    /* A cancelled/lost attempt may have durably stored the
                       start of one UTF-8 character. It has no textual value;
                       the status message still identifies the partial reply. */
                    if (turn.phase != COMPLETED && turn.extent - offset < 4) break;
                    result ← FC_CORRUPT;
                }
                if (result == FC_OK) result ← append_request_text(&body, (const char *)window, length);
                offset ← offset + (result == FC_OK ? length : 0);
            }
            if (result == FC_OK) result ← append_request_bytes(&body, "\"}", 2);
        }
    }
    if (result == FC_OK) result ← append_request_bytes(&body, "]}", 2);
    if (result != FC_OK) { bytes[0] ← 0; return result; }
    *written ← body.length;
    return FC_OK;
}
