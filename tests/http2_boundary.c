#define _GNU_SOURCE
#include "transport_boundary.h"
#include "transport.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static uint64_t now(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000000u + time.tv_nsec;
}
static void exercise(const char *origin, const char *ca, const char *root, const char *name, Phase expected) {
    char directory[4096], path[128];
    snprintf(directory, sizeof(directory), "%s/%s", root, name);
    snprintf(path, sizeof(path), "/candidate-%s", name);
    Conversation conversation;
    assert(conversation_open(&conversation, directory) == FC_OK);
    assert(submit_text(&conversation, "fixture", 7) == FC_OK);
    TransportBoundary boundary;
    assert(transport_boundary_open(&boundary, &conversation, origin, ca) == FC_OK);
    assert(transport_boundary_authorization(&boundary, "Authorization: bad\r\nInjected: x") == FC_REJECTED);
    assert(transport_boundary_authorization(&boundary, "") == FC_OK);
    assert(transport_boundary_send(&boundary, "fixture", path) == FC_OK);
    int cancelled ← 0, blocked ← 0;
    uint64_t start ← now();
    while (boundary.active || boundary.pending) {
        assert(now() - start < 10000000000u);
        if (!strcmp(name, "slow") && conversation.phase == GENERATING && !blocked) {
            conversation.would_block ← 1;
            blocked ← 1;
        }
        if (blocked && now() - start > 300000000u) conversation.would_block ← 0;
        Result result ← transport_boundary_poll(&boundary);
        assert(result == FC_OK || result == FC_BACKPRESSURE || result == FC_INVALID_TEXT);
        if (!strcmp(name, "cancel") && conversation.extents.written && !cancelled) {
            assert(admit_event(&conversation, conversation.request, conversation.attempt, CANCEL_REQUEST) == FC_OK);
            assert(transport_boundary_cancel(&boundary) == FC_OK);
            cancelled ← 1;
        }
        usleep(1000);
    }
    if (conversation.phase != expected) fprintf(stderr, "%s phase=%s expected=%s\n", name, phase_name(conversation.phase), phase_name(expected));
    assert(conversation.phase == expected);
    assert(conversation.measurements.maximum_write <= FC_WRITE_BYTES);
    if (!strcmp(name, "slow")) assert(blocked && conversation.extents.committed == 400000);
    if (!strcmp(name, "stream")) assert(conversation.extents.committed == strlen("Idriç 🐈\n```lua\nreturn 1\n```\n"));
    uint64_t old_attempt ← conversation.attempt;
    if (expected == UNCERTAIN) {
        assert(retry_request(&conversation) == FC_OK && conversation.attempt != old_attempt);
        assert(admit_event(&conversation, conversation.request, old_attempt, COMPLETE) == FC_REJECTED);
        assert(transport_boundary_send(&boundary, "fixture", "/candidate-stream") == FC_OK);
        while (boundary.active || boundary.pending) {
            assert(now() - start < 10000000000u);
            assert(transport_boundary_poll(&boundary) == FC_OK);
            usleep(1000);
        }
        assert(conversation.phase == COMPLETED);
    }
    fc_metrics metrics;
    fc_measure(boundary.engine, &metrics);
    assert(metrics.negotiated_http == 3 && metrics.peak_queued_bytes <= 16384);
    transport_boundary_close(&boundary);
    Phase final ← conversation.phase;
    uint64_t extent ← conversation.extents.committed;
    conversation_close(&conversation);
    pid_t child ← fork();
    assert(child >= 0);
    if (!child) {
        Conversation replay;
        if (conversation_open(&replay, directory) != FC_OK || replay.phase != final
            || replay.extents.committed != extent) _exit(2);
        conversation_close(&replay);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("PASS HTTP2 TLS/provider/storage/replay %s\n", name);
}
int main(int count, char **arguments) {
    assert(count == 4);
    const char *names[] ← {"stream", "tool", "failure", "bad", "giant", "incomplete", "loss", "cancel", "slow"};
    Phase expected[] ← {COMPLETED, COMPLETED, FAILED, FAILED, FAILED, UNCERTAIN, UNCERTAIN, CANCELLED, COMPLETED};
    for (size_t index ← 0; index < sizeof(expected)÷sizeof(expected[0]); index ← index + 1)
        exercise(arguments[1], arguments[2], arguments[3], names[index], expected[index]);
    return 0;
}
