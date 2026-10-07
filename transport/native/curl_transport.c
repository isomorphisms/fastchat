#include "transport.h"
#include "provider_framing.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum { ACTIVE_LIMIT = 2, RECEIVE_LIMIT = 65536, BODY_LIMIT = 65536,
       URL_LIMIT = 2048, HEADER_LIMIT = 16384 };
typedef struct {
  CURL *http;
  struct curl_slist *headers;
  uint64_t request, attempt;
  unsigned char bytes[RECEIVE_LIMIT];
  char body[BODY_LIMIT], url[URL_LIMIT];
  size_t read_at, count, header_bytes;
  int occupied, attached, paused, started, start_pending, terminal;
  int invalid_headers, provider_done, measured;
  fc_sse framing;
} request_channel;
struct fc_transport {
  CURLM *engine;
  char origin[URL_LIMIT], ca[URL_LIMIT];
  int protocol, next_channel, invalidate_cache;
  request_channel requests[ACTIVE_LIMIT];
  fc_metrics metrics;
  fc_event observed;
};
static void measure_request(fc_transport *, request_channel *);
static CURLM *new_engine(void) {
  CURLM *engine = curl_multi_init();
  if (!engine) return NULL;
  if (curl_multi_setopt(engine, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX) ||
      curl_multi_setopt(engine, CURLMOPT_MAX_HOST_CONNECTIONS, 1L) ||
      curl_multi_setopt(engine, CURLMOPT_MAX_TOTAL_CONNECTIONS, 1L) ||
      curl_multi_setopt(engine, CURLMOPT_MAX_CONCURRENT_STREAMS, 2L)) {
    curl_multi_cleanup(engine); return NULL;
  }
  return engine;
}
static int valid_protocol(fc_transport *t, long version) {
  return version == (t->protocol == 2 ? CURL_HTTP_VERSION_2_0 : CURL_HTTP_VERSION_3);
}
static size_t receive_bytes(char *data, size_t size, size_t number, void *context) {
  request_channel *s = context;
  size_t n = size * number;
  if (n > RECEIVE_LIMIT) return CURL_WRITEFUNC_ERROR;
  if (n > RECEIVE_LIMIT - s->count) { s->paused = 1; return CURL_WRITEFUNC_PAUSE; }
  size_t end = (s->read_at + s->count) % RECEIVE_LIMIT;
  size_t first = n < RECEIVE_LIMIT - end ? n : RECEIVE_LIMIT - end;
  memcpy(s->bytes + end, data, first);
  memcpy(s->bytes, data + first, n - first);
  s->count += n;
  return n;
}
static size_t receive_header(char *data, size_t size, size_t number, void *context) {
  request_channel *s = context;
  size_t n = size * number;
  if (n > HEADER_LIMIT - s->header_bytes) { s->invalid_headers = 1; return 0; }
  s->header_bytes += n;
  if (n >= 17 && !strncasecmp(data, "content-encoding:", 17)) {
    if (n < 26 || strncasecmp(data + 17, " identity", 9)) s->invalid_headers = 1;
  }
  if (n == 2 && !memcmp(data, "\r\n", 2)) {
    long code = 0;
    curl_easy_getinfo(s->http, CURLINFO_RESPONSE_CODE, &code);
    if (code >= 200) {
      if (code >= 300) s->invalid_headers = 1;
      else if (!s->started) { s->started = 1; s->start_pending = 1; }
    }
  }
  return s->invalid_headers ? 0 : n;
}
static void detach(fc_transport *t, request_channel *s) {
  if (s->attached) { curl_multi_remove_handle(t->engine, s->http); s->attached = 0; }
}
static void release(fc_transport *t, request_channel *s) {
  detach(t, s);
  if (s->http) curl_easy_cleanup(s->http);
  curl_slist_free_all(s->headers);
  memset(s, 0, sizeof(*s));
}
fc_transport *fc_open(const char *origin, const char *ca, int version) {
  /* Refuse origin paths/userinfo/query and ambient proxy routing. Credentials
   * and redirect decisions must be explicit authority policy in a later adapter. */
  if (!origin || !ca || (version != 2 && version != 3) ||
      strncmp(origin, "https://", 8) || !origin[8] || strchr(origin + 8, '/') ||
      strchr(origin, '@') || strchr(origin, '?') || strchr(origin, '#') ||
      strlen(origin) >= URL_LIMIT || strlen(ca) >= URL_LIMIT) return NULL;
  const curl_version_info_data *features = curl_version_info(CURLVERSION_NOW);
  if (!(features->features & (version == 2 ? CURL_VERSION_HTTP2 : CURL_VERSION_HTTP3)))
    return NULL;
  fc_transport *t = calloc(1, sizeof(*t));
  if (!t) return NULL;
  strcpy(t->origin, origin); strcpy(t->ca, ca); t->protocol = version;
  t->engine = new_engine();
  if (!t->engine) { free(t); return NULL; }
  return t;
}
void fc_close(fc_transport *t) {
  if (!t) return;
  for (int i = 0; i < ACTIVE_LIMIT; ++i) release(t, &t->requests[i]);
  curl_multi_cleanup(t->engine); free(t);
}
int fc_submit(fc_transport *t, uint64_t request, uint64_t attempt,
              const char *path, const char *body, size_t length) {
  if (!t || !path || path[0] != '/' || path[1] == '/' || !request || !attempt ||
      (!body && length) || length > BODY_LIMIT) return -1;
  if (t->invalidate_cache) {
    int attached = 0;
    for (int i = 0; i < ACTIVE_LIMIT; ++i) attached += t->requests[i].attached;
    if (!attached) {
      CURLM *replacement = new_engine();
      if (!replacement) return -1;
      curl_multi_cleanup(t->engine); t->engine = replacement; t->invalidate_cache = 0;
    }
  }
  for (const char *p = path; *p; ++p) if ((unsigned char)*p <= 32 || *p == '#') return -1;
  request_channel *s = NULL;
  for (int i = 0; i < ACTIVE_LIMIT; ++i) {
    if (t->requests[i].occupied && t->requests[i].request == request) return -1;
    if (!t->requests[i].occupied && !s) s = &t->requests[i];
  }
  if (!s || strlen(t->origin) + strlen(path) >= URL_LIMIT) return -1;
  s->occupied = 1; s->request = request; s->attempt = attempt;
  snprintf(s->url, sizeof(s->url), "%s%s", t->origin, path);
  if (length) memcpy(s->body, body, length);
  s->http = curl_easy_init();
  s->headers = curl_slist_append(NULL, "Content-Type: application/json");
  if (!s->http || !s->headers) { release(t, s); return -1; }
  struct curl_slist *headers = curl_slist_append(s->headers, "Accept: text/event-stream");
  if (!headers) { release(t, s); return -1; } s->headers = headers;
  headers = curl_slist_append(s->headers, "Accept-Encoding: identity");
  if (!headers) { release(t, s); return -1; } s->headers = headers;
#define OPTION(key, value) do { CURLcode result = curl_easy_setopt(s->http, key, value); \
  if (result) { fprintf(stderr, "transport option %s refused: %s\n", #key, curl_easy_strerror(result)); goto failed; } } while (0)
  OPTION(CURLOPT_URL, s->url);
  OPTION(CURLOPT_PROTOCOLS_STR, "https");
  CURLcode proxy = curl_easy_setopt(s->http, CURLOPT_PROXY, "");
  if (proxy != CURLE_OK && proxy != CURLE_NOT_BUILT_IN && proxy != CURLE_UNKNOWN_OPTION) goto failed;
  OPTION(CURLOPT_CAINFO, t->ca);
  OPTION(CURLOPT_SSL_VERIFYPEER, 1L);
  OPTION(CURLOPT_SSL_VERIFYHOST, 2L);
  OPTION(CURLOPT_HTTP_VERSION, t->protocol == 2 ? CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE : CURL_HTTP_VERSION_3ONLY);
  OPTION(CURLOPT_PIPEWAIT, 1L);
  OPTION(CURLOPT_FOLLOWLOCATION, 0L);
  OPTION(CURLOPT_NOSIGNAL, 1L);
  OPTION(CURLOPT_CONNECTTIMEOUT_MS, 5000L);
  OPTION(CURLOPT_TIMEOUT_MS, 30000L);
  OPTION(CURLOPT_POST, 1L);
  OPTION(CURLOPT_POSTFIELDS, s->body);
  OPTION(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)length);
  OPTION(CURLOPT_HTTPHEADER, s->headers);
  OPTION(CURLOPT_WRITEFUNCTION, receive_bytes);
  OPTION(CURLOPT_WRITEDATA, s);
  OPTION(CURLOPT_HEADERFUNCTION, receive_header);
  OPTION(CURLOPT_HEADERDATA, s);
  OPTION(CURLOPT_PRIVATE, s);
  /* No CURLOPT_SSL_ENABLE_EARLY_DATA: state-changing submissions never use 0-RTT. */
#undef OPTION
  if (curl_multi_add_handle(t->engine, s->http)) goto failed;
  s->attached = 1;
  return 0;
failed:
  release(t, s); return -1;
}
int fc_cancel(fc_transport *t, uint64_t request, uint64_t attempt) {
  if (!t) return -1;
  for (int i = 0; i < ACTIVE_LIMIT; ++i) {
    request_channel *s = &t->requests[i];
    if (s->occupied && s->request == request && s->attempt == attempt) {
      if (s->terminal) return 0; /* Already observed terminal wins. */
      measure_request(t, s);
      detach(t, s); s->count = 0; s->start_pending = 0; s->terminal = FC_CANCELLED;
      return 0;
    }
  }
  return -1;
}
static void measure_request(fc_transport *t, request_channel *s) {
  if (s->measured) return;
  s->measured = 1;
  long connections = 0, version = 0;
  curl_off_t handshake = 0, first = 0;
  curl_easy_getinfo(s->http, CURLINFO_NUM_CONNECTS, &connections);
  curl_easy_getinfo(s->http, CURLINFO_HTTP_VERSION, &version);
  curl_easy_getinfo(s->http, CURLINFO_APPCONNECT_TIME_T, &handshake);
  curl_easy_getinfo(s->http, CURLINFO_STARTTRANSFER_TIME_T, &first);
  t->metrics.new_connections += (uint64_t)connections;
  if (version) t->metrics.negotiated_http = version;
  if (handshake) t->metrics.handshake_us = handshake;
  if (first) t->metrics.first_byte_us = first;
}
int fc_step(fc_transport *t, int wait_ms) {
  if (!t || wait_ms < 0 || wait_ms > 1000) return -1;
  int running = 0;
  CURLMcode progressed = curl_multi_perform(t->engine, &running);
  if (progressed) { fprintf(stderr, "HTTP engine failed to advance: %s\n", curl_multi_strerror(progressed)); return -1; }
  int pending = 0; CURLMsg *message;
  while ((message = curl_multi_info_read(t->engine, &pending))) {
    if (message->msg != CURLMSG_DONE) continue;
    CURLcode outcome = message->data.result; /* CURLMsg expires on remove_handle. */
    request_channel *s = NULL;
    curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &s);
    if (!s) return -1;
    measure_request(t, s);
    detach(t, s);
    /* Fail conservatively on network errors even if upload bytes are unknown.
     * A new attempt is always a core decision. No hidden POST retry here. */
    long version = 0;
    curl_easy_getinfo(s->http, CURLINFO_HTTP_VERSION, &version);
    s->terminal = s->invalid_headers || (version && !valid_protocol(t, version)) ? FC_FAILED :
      outcome == CURLE_OK ? FC_COMPLETED : FC_UNCERTAIN;
    if (s->terminal == FC_UNCERTAIN) t->invalidate_cache = 1;
  }
  for (int i = 0; i < ACTIVE_LIMIT; ++i) {
    request_channel *s = &t->requests[i];
    if (s->count > t->metrics.peak_queued_bytes) t->metrics.peak_queued_bytes = s->count;
  }
  if (wait_ms && running && curl_multi_poll(t->engine, NULL, 0, wait_ms, NULL)) return -1;
  return 0;
}
int fc_next(fc_transport *t, fc_event *event) {
  if (!t || !event) return 0;
  for (int offset = 0; offset < ACTIVE_LIMIT; ++offset) {
    int index = (t->next_channel + offset) % ACTIVE_LIMIT;
    request_channel *s = &t->requests[index];
    if (!s->occupied) continue;
    memset(event, 0, sizeof(*event));
    event->request = s->request; event->attempt = s->attempt;
    if (s->start_pending) {
      long version = 0; curl_easy_getinfo(s->http, CURLINFO_HTTP_VERSION, &version);
      s->start_pending = 0;
      if (!valid_protocol(t, version)) { detach(t, s); s->count = 0; s->terminal = FC_FAILED; }
      else event->kind = FC_STARTED;
    }
    while (!event->kind && s->count) {
      unsigned char byte = s->bytes[s->read_at];
      s->read_at = (s->read_at + 1) % RECEIVE_LIMIT; --s->count;
      int framed = fc_provider_byte(&s->framing, byte, event->text, &event->length);
      if (framed < 0) { detach(t, s); s->count = 0; s->terminal = FC_FAILED; }
      else if (framed) {
        if (framed == 2) {
          s->provider_done = 1; s->count = 0; detach(t, s); s->terminal = FC_COMPLETED;
          measure_request(t, s);
        } else event->kind = FC_CHUNK;
      }
    }
    if (s->paused && s->attached && s->count <= RECEIVE_LIMIT / 2) {
      s->paused = 0; ++t->metrics.pauses;
      if (curl_easy_pause(s->http, CURLPAUSE_CONT)) { detach(t, s); s->terminal = FC_UNCERTAIN; }
    }
    if (!event->kind && !s->count && s->terminal) {
      event->kind = (fc_event_kind)s->terminal;
      /* EOF without provider completion is incomplete, never a successful model result. */
      if (event->kind == FC_COMPLETED && !s->provider_done) event->kind = FC_UNCERTAIN;
      release(t, s);
    }
    if (event->kind) { t->next_channel = (index + 1) % ACTIVE_LIMIT; return 1; }
  }
  return 0;
}
void fc_measure(fc_transport *t, fc_metrics *out) { if (t && out) *out = t->metrics; }

/* Narrow polling FFI: copies an observation before exposing decoded text.
 * Signed 32-bit identity lowering is checked in the Idriç adapter. */
void *fc_ffi_open(const char *origin, const char *ca, int version) {
  if (curl_global_init(CURL_GLOBAL_DEFAULT)) return NULL;
  void *session = fc_open(origin, ca, version);
  if (!session) curl_global_cleanup();
  return session;
}
void fc_ffi_close(void *session) { fc_close(session); curl_global_cleanup(); }
int fc_ffi_valid(void *session) { return session != NULL; }
int fc_ffi_submit(void *session, int request, int attempt, const char *path, const char *body) {
  if (request <= 0 || attempt <= 0) return -1;
  return fc_submit(session, (uint64_t)request, (uint64_t)attempt, path, body, strlen(body));
}
int fc_ffi_cancel(void *session, int request, int attempt) {
  if (request <= 0 || attempt <= 0) return -1;
  return fc_cancel(session, (uint64_t)request, (uint64_t)attempt);
}
int fc_ffi_next(void *session) {
  fc_transport *t = session;
  if (!t || !fc_next(t, &t->observed)) return 0;
  t->observed.text[t->observed.length] = 0;
  return t->observed.kind;
}
int fc_ffi_request(void *session) { return (int)((fc_transport *)session)->observed.request; }
int fc_ffi_attempt(void *session) { return (int)((fc_transport *)session)->observed.attempt; }
const char *fc_ffi_text(void *session) { return ((fc_transport *)session)->observed.text; }
