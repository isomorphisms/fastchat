#include "sse.h"
#include <string.h>
static int line_end(fc_sse *s, char *out, size_t *length) {
  if (!s->line_length) {
    if (!s->has_data) return 0;
    memcpy(out, s->data, s->data_length);
    *length = s->data_length;
    s->data_length = 0; s->has_data = 0;
    return 1;
  }
  if (s->line_length >= 5 && !memcmp(s->line, "data:", 5)) {
    size_t start = 5;
    if (start < s->line_length && s->line[start] == ' ') ++start;
    size_t n = s->line_length - start;
    if (s->data_length + n + (size_t)s->has_data >= sizeof(s->data)) return -1;
    if (s->has_data) s->data[s->data_length++] = '\n';
    memcpy(s->data + s->data_length, s->line + start, n);
    s->data_length += n; s->has_data = 1;
  }
  s->line_length = 0;
  return 0;
}
int fc_sse_byte(fc_sse *s, unsigned char byte, char *out, size_t *length) {
  if (byte == '\n' && s->previous_cr) { s->previous_cr = 0; return 0; }
  s->previous_cr = byte == '\r';
  if (byte == '\r' || byte == '\n') return line_end(s, out, length);
  if (s->line_length == sizeof(s->line)) return -1;
  s->line[s->line_length++] = (char)byte;
  return 0;
}
