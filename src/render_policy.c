#include "render_policy.h"
int renderer_follows_prefixes(void) { return 0; }
Result render_stored_window(Conversation *conversation, uint64_t offset,
                            unsigned char *bytes, size_t capacity, size_t *length) {
    return read_response_window(conversation, offset, renderer_follows_prefixes(), bytes, capacity, length);
}

