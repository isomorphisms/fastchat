#ifndef FASTCHAT_PROVIDER_FRAMING_H
#define FASTCHAT_PROVIDER_FRAMING_H
#include "sse.h"
/* Fixture text/SSE provider. Independent of every HTTP engine and byte channel.
 * Production JSON interpretation is a separate provider adapter. */
int fc_provider_byte(fc_sse *, unsigned char, char *, size_t *);
#endif
