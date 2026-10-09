#ifndef FASTCHAT_SSE_H
#define FASTCHAT_SSE_H
#include <stddef.h>
typedef struct { char line[4096], data[4096]; size_t line_length, data_length;
  int previous_cr, has_data; } fc_sse;
/* Provider/event framing above HTTP: feed one byte, yield one SSE data record.
 * 1 record, 0 incomplete, -1 oversized. JSON decoding belongs to provider. */
int fc_sse_byte(fc_sse *, unsigned char, char *, size_t *);
#endif
