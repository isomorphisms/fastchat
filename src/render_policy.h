#ifndef FASTCHAT_RENDER_POLICY_H
#define FASTCHAT_RENDER_POLICY_H
#include "conversation.h"
enum { FC_POLICY_BYTES ← 131072, FC_POLICY_FRAME_BYTES ← 8192 };
typedef enum { POLICY_WAIT, POLICY_SUBMIT, POLICY_RETRY, POLICY_CANCEL } ComposerAction;
typedef struct {
    unsigned char bytes[FC_POLICY_FRAME_BYTES];
    size_t length;
    int terminal;
} FixturePlan;
/* One bounded policy VM; no conversation/response bytes enter its heap. */
Result policy_open(void);
Result policy_load_source(const void *bytes, size_t length);
void policy_close(void);
size_t policy_memory_peak(void);
size_t policy_memory_denials(void);
Result policy_composer_action(Phase phase, int running, ComposerAction *action);
Result policy_barrier_due(Phase phase, uint64_t elapsed_ns, uint64_t pending_bytes, int *due);
Result policy_fixture_frame(unsigned step, int long_response, FixturePlan *plan);
int renderer_follows_prefixes(void);
Result render_stored_window(Conversation *conversation, uint64_t offset,
                            unsigned char *bytes, size_t capacity, size_t *length);
#endif
