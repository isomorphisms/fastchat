#include "provider_framing.h"
#include <string.h>
static int valid_text(const unsigned char *text, size_t length) {
  for (size_t i = 0; i < length;) {
    unsigned int code = text[i++], minimum;
    int continuation;
    if (!code) return 0; /* Text FFI is NUL terminated. */
    if (code < 128) continue;
    if (code >= 0xc2 && code <= 0xdf) { continuation = 1; minimum = 0x80; code &= 31; }
    else if (code >= 0xe0 && code <= 0xef) { continuation = 2; minimum = 0x800; code &= 15; }
    else if (code >= 0xf0 && code <= 0xf4) { continuation = 3; minimum = 0x10000; code &= 7; }
    else return 0;
    if ((size_t)continuation > length - i) return 0;
    while (continuation--) {
      unsigned int byte = text[i++];
      if ((byte & 0xc0) != 0x80) return 0;
      code = (code << 6) | (byte & 63);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return 0;
  }
  return 1;
}
int fc_provider_byte(fc_sse *state, unsigned char byte, char *text, size_t *length) {
  int result = fc_sse_byte(state, byte, text, length);
  if (result != 1) return result;
  if (!valid_text((const unsigned char *)text, *length)) return -1;
  return *length == 6 && !memcmp(text, "[DONE]", 6) ? 2 : 1;
}
