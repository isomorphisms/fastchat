#define _GNU_SOURCE
#include "transport_boundary.h"
#include "transport.h"
#include "settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
    TransportBoundary boundary;
    assert(transport_boundary_open(&boundary, &conversation, origin, ca) == FC_OK);
    if (!strcmp(name, "responses")) boundary.request_format ← PROVIDER_RESPONSES;
    AttemptProvider provider;
    assert(transport_boundary_provider(&boundary, "fixture", path, &provider) == FC_OK);
    assert(submit_text_to(&conversation, "fixture", 7, &provider) == FC_OK);
    assert(transport_boundary_send(&boundary, "different-model", path) == FC_REJECTED);
    assert(transport_boundary_send(&boundary, "fixture", "/wrong-path") == FC_REJECTED);
    boundary.request_format ← !strcmp(name, "responses") ? PROVIDER_CHAT_COMPLETIONS : PROVIDER_RESPONSES;
    assert(transport_boundary_send(&boundary, "fixture", path) == FC_REJECTED);
    boundary.request_format ← !strcmp(name, "responses") ? PROVIDER_RESPONSES : PROVIDER_CHAT_COMPLETIONS;
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
        assert(transport_boundary_provider(&boundary, "fixture", "/candidate-stream", &provider) == FC_OK);
        assert(retry_request_to(&conversation, &provider) == FC_OK && conversation.attempt != old_attempt);
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
static void exercise_context(const char *origin, const char *ca, const char *root, ProviderRequestFormat format) {
    char directory[4096];
    const char *name ← format == PROVIDER_RESPONSES ? "context-responses" : "context-chat";
    const char *endpoint ← format == PROVIDER_RESPONSES ? "/candidate-context-responses" : "/candidate-context-chat";
    snprintf(directory, sizeof(directory), "%s/%s", root, name);
    for (unsigned turn ← 0; turn < 2; turn++) {
        Conversation conversation;
        assert(conversation_open(&conversation, directory) == FC_OK);
        if (turn) assert(conversation.phase == COMPLETED && conversation.request == 1);
        const char *prompt ← turn ? "follow-up λ" : "fixture";
        TransportBoundary boundary;
        assert(transport_boundary_open(&boundary, &conversation, origin, ca) == FC_OK);
        boundary.request_format ← format;
        AttemptProvider provider;
        assert(transport_boundary_provider(&boundary, "fixture", endpoint, &provider) == FC_OK);
        assert(submit_text_to(&conversation, prompt, strlen(prompt), &provider) == FC_OK);
        assert(transport_boundary_send(&boundary, "fixture", endpoint) == FC_OK);
        uint64_t start ← now();
        while (boundary.active || boundary.pending) {
            assert(now() - start < 10000000000u);
            assert(transport_boundary_poll(&boundary) == FC_OK);
            usleep(1000);
        }
        assert(conversation.phase == COMPLETED);
        transport_boundary_close(&boundary);
        conversation_close(&conversation);
    }
    printf("PASS HTTP2 TLS two-turn durable replay/context %s\n", name);
}
static void yente_turn(TransportBoundary *boundary, const char *model, const char *path,
                       ProviderRequestFormat format, int retry, Phase expected) {
    Conversation *conversation ← boundary->provider.conversation;
    boundary->request_format ← format;
    AttemptProvider provider;
    assert(transport_boundary_provider(boundary, model, path, &provider) == FC_OK);
    if (retry) assert(retry_request_to(conversation, &provider) == FC_OK);
    else assert(submit_text_to(conversation, "/wish tea", 9, &provider) == FC_OK);
    assert(transport_boundary_send(boundary, model, path) == FC_OK);
    uint64_t start ← now();
    while (boundary->active || boundary->pending) {
        assert(now() - start < 10000000000u);
        assert(transport_boundary_poll(boundary) == FC_OK);
        usleep(1000);
    }
    assert(conversation->phase == expected);
}
static void exercise_yente(const char *origin, const char *ca, const char *root, int restarted) {
    char directory[4096], second[4096], corrupt[4096];
    snprintf(directory, sizeof(directory), "%s/yente-first", root);
    snprintf(second, sizeof(second), "%s/yente-second", root);
    snprintf(corrupt, sizeof(corrupt), "%s/yente-corrupt", root);
    const char *chat ← "/v1/chat/completions/fictional";
    const char *responses ← "/v1/responses/fictional";
    const char *authorization ← "Authorization: Bearer fixture-native-only-token";
    Conversation conversation;
    TransportBoundary boundary;
    assert(conversation_open(&conversation, directory) == FC_OK);
    assert(transport_boundary_open(&boundary, &conversation, origin, ca) == FC_OK);
    assert(transport_boundary_authorization(&boundary, authorization) == FC_OK);
    if (!restarted) {
        assert(conversation.request == 0);
        yente_turn(&boundary, "yente-mock", chat, PROVIDER_CHAT_COMPLETIONS, 0, UNCERTAIN);
        yente_turn(&boundary, "yente-mock", chat, PROVIDER_CHAT_COMPLETIONS, 1, UNCERTAIN);
        assert(conversation.request == 1 && conversation.attempt == 2);
    } else {
        assert(conversation.phase == UNCERTAIN && conversation.request == 1 && conversation.attempt == 2);
        yente_turn(&boundary, "yente-mock", chat, PROVIDER_CHAT_COMPLETIONS, 1, COMPLETED);
        yente_turn(&boundary, "yente-mock", responses, PROVIDER_RESPONSES, 0, COMPLETED);
        yente_turn(&boundary, "fixture", chat, PROVIDER_CHAT_COMPLETIONS, 0, COMPLETED);
        yente_turn(&boundary, "yente-mock", "/candidate-ordinary", PROVIDER_CHAT_COMPLETIONS, 0, COMPLETED);
    }
    char nonce_path[4096];
    int name_size ← snprintf(nonce_path, sizeof(nonce_path), "%s/yente-journal-nonce", directory);
    assert(name_size > 0 && (size_t)name_size < sizeof(nonce_path));
    struct stat nonce;
    assert(stat(nonce_path, &nonce) == 0 && (nonce.st_mode & 0777) == 0600 && nonce.st_size == 32);
    transport_boundary_close(&boundary);
    conversation_close(&conversation);
    if (!restarted) return;
    assert(conversation_open(&conversation, second) == FC_OK);
    assert(transport_boundary_open(&boundary, &conversation, origin, ca) == FC_OK);
    assert(transport_boundary_authorization(&boundary, authorization) == FC_OK);
    yente_turn(&boundary, "yente-mock", responses, PROVIDER_RESPONSES, 0, COMPLETED);
    transport_boundary_close(&boundary);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, corrupt) == FC_OK);
    assert(private_file_write(corrupt, "yente-journal-nonce", "", 0) == FC_OK);
    assert(transport_boundary_open(&boundary, &conversation, origin, ca) == FC_OK);
    AttemptProvider provider;
    assert(transport_boundary_provider(&boundary, "yente-mock", chat, &provider) == FC_OK);
    assert(submit_text_to(&conversation, "/wish tea", 9, &provider) == FC_OK);
    assert(transport_boundary_send(&boundary, "yente-mock", chat) == FC_CORRUPT && !boundary.active);
    assert(fc_submit_with_event_id(boundary.engine, 1, 1, chat, "{}", 2, "not a valid token") != 0);
    transport_boundary_close(&boundary);
    conversation_close(&conversation);
    puts("PASS Yente durable retry and new requests; damaged nonce and invalid header refused");
}
int main(int count, char **arguments) {
    assert(count == 4 || count == 5);
    if (count == 5) {
        assert(!strcmp(arguments[4], "yente-start") || !strcmp(arguments[4], "yente-restart"));
        exercise_yente(arguments[1], arguments[2], arguments[3], !strcmp(arguments[4], "yente-restart"));
        return 0;
    }
    const char *names[] ← {"stream", "tool", "failure", "bad", "giant", "incomplete", "loss", "cancel", "slow", "responses", "usage"};
    Phase expected[] ← {COMPLETED, COMPLETED, FAILED, FAILED, FAILED, UNCERTAIN, UNCERTAIN, CANCELLED, COMPLETED, COMPLETED, COMPLETED};
    for (size_t index ← 0; index < sizeof(expected)÷sizeof(expected[0]); index ← index + 1)
        exercise(arguments[1], arguments[2], arguments[3], names[index], expected[index]);
    exercise_context(arguments[1], arguments[2], arguments[3], PROVIDER_CHAT_COMPLETIONS);
    exercise_context(arguments[1], arguments[2], arguments[3], PROVIDER_RESPONSES);
    return 0;
}
