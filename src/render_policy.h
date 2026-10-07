#ifndef FASTCHAT_RENDER_POLICY_H
#define FASTCHAT_RENDER_POLICY_H
#include "conversation.h"
int renderer_follows_prefixes(void);
Result render_stored_window(Conversation *conversation, uint64_t offset,
                            unsigned char *bytes, size_t capacity, size_t *length);
#endif
