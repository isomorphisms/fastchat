#ifndef FASTCHAT_TRANSPORT_H
#define FASTCHAT_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>

/* Foreign ABI only. The conversation uses FastChatCore's distinct types. */
typedef struct fc_transport fc_transport;
typedef enum {
  FC_STARTED = 1, FC_CHUNK, FC_COMPLETED, FC_CANCELLED, FC_FAILED, FC_UNCERTAIN
} fc_event_kind;
typedef struct {
  uint64_t request, attempt;
  fc_event_kind kind;
  size_t length;
  char text[4096];
} fc_event;
typedef struct {
  uint64_t new_connections, pauses, peak_queued_bytes;
  long negotiated_http;
  int64_t handshake_us, first_byte_us;
} fc_metrics;

/* One authority/origin per engine. HTTPS, certificate verification mandatory.
 * All calls on its single owner thread; no implicit provider retry. */
fc_transport *fc_open(const char *origin, const char *ca_file, int http_version);
void fc_close(fc_transport *);
int fc_submit(fc_transport *, uint64_t request, uint64_t attempt,
              const char *path, const char *body, size_t body_length);
int fc_cancel(fc_transport *, uint64_t request, uint64_t attempt);
int fc_step(fc_transport *, int maximum_wait_ms);
/* 1 event, 0 none. Call regularly to release receive credit. */
int fc_next(fc_transport *, fc_event *);
void fc_measure(fc_transport *, fc_metrics *);
#endif
