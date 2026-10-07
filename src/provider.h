#ifndef FASTCHAT_PROVIDER_H
#define FASTCHAT_PROVIDER_H
#include "conversation.h"
enum { FC_PROVIDER_BYTES ← 8192, FC_JSON_TOKENS ← 256 };
typedef struct {
    Conversation *conversation;
    uint64_t request, attempt;
    unsigned char pending[FC_PROVIDER_BYTES];
    size_t length, written;
    char tool[FC_PROMPT_BYTES];
    size_t tool_length;
    Event terminal;
    int poisoned;
} Provider;
void provider_open(Provider *, Conversation *);
Result provider_record(Provider *, const char *json, size_t length);
Result provider_resume(Provider *);
Result provider_terminal(Provider *, Event);
Result provider_decode_fixture_text(const unsigned char *, size_t, unsigned char *, size_t *);
/* Credential-free request adaptation. Authorization belongs to transport. */
Result provider_request_body(const char *model, const char *prompt, size_t length,
                             char *body, size_t capacity, size_t *written);
#endif
