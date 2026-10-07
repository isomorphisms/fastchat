#include "provider_framing.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int feed(fc_sse *s, const char *bytes, size_t n, char *out, size_t *length) {
  int result = 0;
  for (size_t i = 0; i < n; ++i) { int next = fc_provider_byte(s, (unsigned char)bytes[i], out, length); if (next) result = next; }
  return result;
}
int main(void) {
  fc_sse state = {0}; char text[4096]; size_t length = 0;
  const char *record = ": ignored\r\ndata: first\r\ndata: Idriç\r\n\r\n";
  assert(feed(&state, record, strlen(record), text, &length) == 1);
  assert(length == strlen("first\nIdriç") && !memcmp(text, "first\nIdriç", length));
  assert(feed(&state, "data: [DONE]\n\n", 14, text, &length) == 2);
  memset(&state, 0, sizeof(state));
  assert(feed(&state, "data: \xc0\x80\n\n", 10, text, &length) == -1);
  memset(&state, 0, sizeof(state));
  assert(feed(&state, "data: a\0b\n\n", 11, text, &length) == -1);
  memset(&state, 0, sizeof(state));
  for (int i = 0; i < 4096; ++i) assert(fc_provider_byte(&state, 'x', text, &length) == 0);
  assert(fc_provider_byte(&state, 'x', text, &length) == -1);
  puts("PASS byte-split SSE, multiline Unicode, completion, malformed text and bounds");
}
