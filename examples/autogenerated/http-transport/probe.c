#define _POSIX_C_SOURCE 200809L
#include "transport.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <unistd.h>
static double milliseconds(void) {
  struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}
static long resident_kib(void) {
  FILE *status = fopen("/proc/self/status", "r");
  if (!status) return -1;
  char line[256]; long value = -1;
  while (fgets(line, sizeof(line), status)) if (sscanf(line, "VmRSS: %ld kB", &value) == 1) break;
  fclose(status); return value;
}
int main(int argc, char **argv) {
  if (argc != 7) { fprintf(stderr, "usage: probe origin ca protocol path1 path2 mode\n"); return 2; }
  if (curl_global_init(CURL_GLOBAL_DEFAULT)) return 2;
  fc_transport *t = fc_open(argv[1], argv[2], atoi(argv[3]));
  if (!t) { fprintf(stderr, "BLOCKED selected protocol/authority unavailable\n"); return 2; }
  const char *payload = "{\"prompt\":\"fixture\"}";
  if (fc_submit(t, 1, 1, argv[4], payload, strlen(payload)) ||
      fc_submit(t, 2, 2, argv[5], payload, strlen(payload))) return 2;
  int slow = !strcmp(argv[6], "slow"), cancelled = 0, finished = 0, retried = 0;
  double start = milliseconds(), first = 0, cancel_at = 0;
  int started = 0; long active_rss = -1;
  fc_event event;
  while (finished < 2 && milliseconds() - start < 35000) {
    if (fc_step(t, 5)) return 3;
    if (slow && milliseconds() - start < 500) continue;
    if (fc_next(t, &event)) {
      if (event.kind == FC_STARTED && ++started == 2) active_rss = resident_kib();
      if (!first) first = milliseconds() - start;
      printf("event\t%llu\t%llu\t%d\t", (unsigned long long)event.request,
             (unsigned long long)event.attempt, event.kind);
      /* Fixture data is escaped to one TSV field; model payload remains bytes. */
      for (size_t i = 0; i < event.length; ++i) {
        unsigned char byte = (unsigned char)event.text[i];
        if (byte == '\n') fputs("\\n", stdout);
        else if (byte == '\r') fputs("\\r", stdout);
        else if (byte == '\t') fputs("\\t", stdout);
        else if (byte == '\\') fputs("\\\\", stdout);
        else if (byte < 32) printf("\\x%02x", byte);
        else putchar(byte);
      }
      putchar('\n');
      if (!strcmp(argv[6], "cancel") && event.request == 1 && event.kind == FC_CHUNK && !cancelled) {
        cancel_at = milliseconds();
        if (fc_cancel(t, 1, 1)) return 3;
        cancelled = 1;
      }
      if (event.kind >= FC_COMPLETED) {
        ++finished;
        if (event.kind == FC_CANCELLED) printf("cancel_latency_ms\t%.3f\n", milliseconds() - cancel_at);
      }
      if (finished == 2 && !strcmp(argv[6], "retry") && !retried) {
        if (fc_submit(t, 1, 3, "/stream", payload, strlen(payload)) ||
            fc_submit(t, 2, 4, "/stream", payload, strlen(payload))) return 3;
        finished = 0; retried = 1;
      }
      if (slow) { struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL); }
    }
  }
  fc_metrics metrics; fc_measure(t, &metrics);
  struct rusage usage; getrusage(RUSAGE_SELF, &usage);
  printf("metrics\tconnections=%llu\tpauses=%llu\tpeak_queue=%llu\thttp=%ld\thandshake_us=%lld\tfirst_byte_us=%lld\tfirst_event_ms=%.3f\twall_ms=%.3f\tpeak_rss_kib=%ld\tactive_rss_kib=%ld\tidle_rss_kib=%ld\tcpu_us=%ld\n",
         (unsigned long long)metrics.new_connections, (unsigned long long)metrics.pauses,
         (unsigned long long)metrics.peak_queued_bytes, metrics.negotiated_http,
         (long long)metrics.handshake_us, (long long)metrics.first_byte_us, first,
         milliseconds() - start, usage.ru_maxrss, active_rss, resident_kib(),
         usage.ru_utime.tv_sec * 1000000 + usage.ru_utime.tv_usec +
         usage.ru_stime.tv_sec * 1000000 + usage.ru_stime.tv_usec);
  fc_close(t); curl_global_cleanup();
  return finished == 2 ? 0 : 4;
}
