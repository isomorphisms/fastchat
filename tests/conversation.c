#define _GNU_SOURCE
#include "conversation.h"
#include "fixture_transport.h"
#include "render_policy.h"
#include "provider.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct { unsigned char *bytes; size_t length, capacity; int finished; } RamControl;
static int ram_write(RamControl *sink, const void *bytes, size_t length) {
    if (sink->finished) return 0;
    if (length > sink->capacity - sink->length) {
        size_t capacity ← sink->capacity ? sink->capacity : 4096;
        while (capacity - sink->length < length) capacity ← capacity * 2;
        unsigned char *storage ← realloc(sink->bytes, capacity);
        if (!storage) return 0;
        sink->bytes ← storage;
        sink->capacity ← capacity;
    }
    memcpy(sink->bytes + sink->length, bytes, length);
    sink->length ← sink->length + length;
    return 1;
}
static void ram_finish(RamControl *sink) { sink->finished ← 1; }
static double clock_seconds(clockid_t clock) {
    struct timespec time;
    assert(clock_gettime(clock, &time) == 0);
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}
static void remove_directory(const char *path) {
    DIR *directory ← opendir(path);
    assert(directory);
    struct dirent *entry;
    while ((entry ← readdir(directory))) {
        if (entry->d_name[0] != '.') assert(unlinkat(dirfd(directory), entry->d_name, 0) == 0);
    }
    assert(closedir(directory) == 0);
    assert(rmdir(path) == 0);
}
static void fresh_conversation(Conversation *conversation, char *path) {
    strcpy(path, "/tmp/fastchat-test-XXXXXX");
    assert(mkdtemp(path));
    assert(conversation_open(conversation, path) == FC_OK);
    assert(submit_text(conversation, "hello", 5) == FC_OK);
    assert(admit_event(conversation, conversation->request, conversation->attempt, START) == FC_OK);
}
static void finish_conversation(Conversation *conversation) {
    assert(admit_event(conversation, conversation->request, conversation->attempt, COMPLETE) == FC_OK);
    assert(conversation->phase == COMPLETED);
    assert(extents_are_ordered(conversation));
}
static void expect_bytes(Conversation *conversation, const unsigned char *expected, size_t length) {
    unsigned char actual[FC_VIEW_BYTES];
    uint64_t offset ← 0;
    while (offset < length) {
        size_t count;
        assert(read_response_window(conversation, offset, 0, actual, sizeof(actual), &count) == FC_OK);
        assert(count && count <= length - offset);
        assert(memcmp(actual, expected + offset, count) == 0);
        offset ← offset + count;
    }
    assert(conversation->extents.committed == length);
}
static void feed_bytes(Conversation *conversation, const unsigned char *bytes, size_t length) {
    WriteResult written ← store_response_bytes(conversation, conversation->request,
                                               conversation->attempt, bytes, length);
    assert(written.result == FC_OK && written.consumed == length);
    assert(extents_are_ordered(conversation));
}
static void prefix_visibility_case(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    unsigned char bytes[FC_BATCH_BYTES];
    memset(bytes, 'a', sizeof(bytes));
    bytes[FC_BATCH_BYTES - 1] ← 0xe2;
    feed_bytes(&conversation, bytes, sizeof(bytes));
    assert(conversation.extents.committed == FC_BATCH_BYTES);
    unsigned char view[FC_VIEW_BYTES];
    size_t length;
    assert(render_stored_window(&conversation, 0, view, sizeof(view), &length) == FC_OK);
    assert(length == (renderer_follows_prefixes() ? FC_VIEW_BYTES : 0));
    assert(render_stored_window(&conversation, FC_BATCH_BYTES - 1, view, sizeof(view), &length) == FC_OK);
    assert(length == 0);
    static const unsigned char continuation[] ← { 0x82, 0xac };
    feed_bytes(&conversation, continuation, sizeof(continuation));
    /* Written UTF-8 continuation is ineligible until the next durable prefix. */
    assert(render_stored_window(&conversation, FC_BATCH_BYTES - 1, view, sizeof(view), &length) == FC_OK);
    assert(length == 0);
    assert(commit_stored_prefix(&conversation) == FC_OK);
    assert(render_stored_window(&conversation, FC_BATCH_BYTES - 1, view, sizeof(view), &length) == FC_OK);
    assert(length == (renderer_follows_prefixes() ? 3u : 0u));
    finish_conversation(&conversation);
    assert(render_stored_window(&conversation, FC_BATCH_BYTES - 1, view, sizeof(view), &length) == FC_OK);
    assert(length == 3 && memcmp(view, "\xe2\x82\xac", 3) == 0);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS branch renderer policy, committed prefixes, UTF-8 visibility eligibility");
}
static void response_cases(void) {
    static const unsigned char hostile[] ← "A\xe2\x82\xac\xf0\x9f\x99\x82\n";
    for (size_t length ← 0; length <= sizeof(hostile) - 1; length ← length + 1) {
        /* Every byte cut in a valid body, not a fixture that truncates the body. */
        Conversation conversation;
        char path[64];
        fresh_conversation(&conversation, path);
        feed_bytes(&conversation, hostile, length);
        feed_bytes(&conversation, hostile + length, sizeof(hostile) - 1 - length);
        unsigned char window[FC_VIEW_BYTES];
        size_t shown;
        assert(read_response_window(&conversation, 0, 0, window, sizeof(window), &shown) == FC_OK);
        assert(shown == 0);
        finish_conversation(&conversation);
        expect_bytes(&conversation, hostile, sizeof(hostile) - 1);
        uint64_t events ← conversation.sequence;
        assert(admit_event(&conversation, conversation.request, conversation.attempt, COMPLETE) == FC_REJECTED);
        assert(conversation.sequence == events);
        conversation_close(&conversation);
        assert(conversation_open(&conversation, path) == FC_OK);
        assert(conversation.phase == COMPLETED && conversation.sequence == events);
        expect_bytes(&conversation, hostile, sizeof(hostile) - 1);
        conversation_close(&conversation);
        remove_directory(path);
    }
    Conversation empty;
    char path[64];
    fresh_conversation(&empty, path);
    feed_bytes(&empty, NULL, 0);
    finish_conversation(&empty);
    assert(empty.extents.committed == 0);
    conversation_close(&empty);
    assert(conversation_open(&empty, path) == FC_OK && empty.phase == COMPLETED);
    conversation_close(&empty);
    remove_directory(path);
    puts("PASS empty, one/many chunks, every UTF-8 cut, disk-first visibility, duplicate terminal, completed replay");
}
static void arena_reuse_case(void) {
    Conversation conversation;
    char path[64];
    unsigned char first[257];
    static const unsigned char second[] ← "second";
    memset(first, 'r', sizeof(first));

    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, first, sizeof(first));
    finish_conversation(&conversation);

    uint64_t first_end ← conversation.response_base + conversation.extents.committed;
    uint64_t arena_capacity ← conversation.arena_capacity;
    uint64_t reserve_calls ← conversation.measurements.reserve_calls;
    assert(arena_capacity > first_end);
    assert(reserve_calls > 0);
    assert(conversation.measurements.allocation_bytes == arena_capacity);
    assert(conversation.measurements.allocation_calls > 0);
    assert(conversation.measurements.reserve_bytes == 0);

    assert(submit_text(&conversation, "again", 5) == FC_OK);
    assert(admit_event(&conversation, conversation.request,
                       conversation.attempt, START) == FC_OK);
    assert(conversation.response_base == first_end);
    assert(conversation.arena_capacity == arena_capacity);
    assert(conversation.extents.capacity == arena_capacity - first_end);

    feed_bytes(&conversation, second, sizeof(second) - 1);
    finish_conversation(&conversation);
    assert(conversation.arena_capacity == arena_capacity);
    assert(conversation.measurements.reserve_calls == reserve_calls);
    expect_bytes(&conversation, second, sizeof(second) - 1);

    uint64_t second_base ← conversation.response_base;
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK);
    assert(conversation.phase == COMPLETED);
    assert(conversation.response_base == second_base);
    assert(conversation.arena_capacity == arena_capacity);
    expect_bytes(&conversation, second, sizeof(second) - 1);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS shared response arena reuses reserved tail across attempts and replay");
}
static uint64_t record_number(const unsigned char *bytes, size_t length) {
    uint64_t value ← 0;
    for (size_t index ← length; index > 0; index ← index - 1) value ← (value << 8) | bytes[index-1];
    return value;
}
static void set_record_number(unsigned char *bytes, uint64_t value, size_t length) {
    for (size_t index ← 0; index < length; index ← index + 1) { bytes[index] ← (unsigned char)value; value ← value >> 8; }
}
static void native_retry_order_case(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, TRANSPORT_LOSS) == FC_OK);
    uint64_t before ← conversation.journal_end;
    uint64_t old_attempt ← conversation.attempt;
    uint64_t old_request ← conversation.request;
    assert(retry_request(&conversation) == FC_OK);
    unsigned char user[64], submitted[64];
    assert(pread(conversation.journal, user, 64, (off_t)before) == 64);
    /* Check the actual record kind and identities, not an event count. */
    if (record_number(user+44, 4) != RETRY || strcmp(event_name((Event)record_number(user+44, 4)), "retry_requested")) {
        fputs("FAIL missing canonical retry_requested before transport_attempt_submitted\n", stderr);
        exit(91);
    }
    assert(pread(conversation.journal, submitted, 64, (off_t)(before+64)) == 64);
    assert(record_number(submitted+44, 4) == ATTEMPT_SUBMIT);
    assert(record_number(user+16, 8) == old_request && record_number(user+24, 8) == old_attempt);
    assert(record_number(submitted+16, 8) == old_request && record_number(submitted+24, 8) == old_attempt+1);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == UNCERTAIN);
    assert(conversation.attempt == old_attempt+1);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS canonical retry_requested record precedes fresh transport_attempt_submitted with exact identities");
}
static void arena_hostile_cases(void) {
    Conversation conversation;
    char path[64];
    for (int corruption ← 0; corruption < 5; corruption ← corruption + 1) {
        fresh_conversation(&conversation, path);
        feed_bytes(&conversation, (const unsigned char *)"first immutable body", 20);
        finish_conversation(&conversation);
        uint64_t first_end ← conversation.arena_high_water;
        assert(submit_text(&conversation, "second", 6) == FC_OK);
        assert(conversation.response_base == first_end);
        assert(admit_event(&conversation, conversation.request, conversation.attempt, START) == FC_OK);
        feed_bytes(&conversation, (const unsigned char *)"second body", 11);
        finish_conversation(&conversation);
        if (corruption == 0) assert(pwrite(conversation.response, "X", 1, 0) == 1);
        if (corruption == 1) assert(ftruncate(conversation.response, (off_t)(first_end+10)) == 0);
        if (corruption >= 2) {
            unsigned char record[64];
            uint64_t offset ← conversation.journal_end - 64;
            assert(pread(conversation.journal, record, 64, (off_t)offset) == 64);
            if (corruption == 2) set_record_number(record+32, first_end-1, 8);
            if (corruption == 3) set_record_number(record+24, conversation.attempt-1, 8);
            if (corruption == 4) memcpy(record, "FC01", 4);
            memset(record+60, 0, 4);
            set_record_number(record+60, checksum_bytes(0xffffffffu, record, 64), 4);
            assert(pwrite(conversation.journal, record, 64, (off_t)offset) == 64);
        }
        assert(fsync(conversation.response) == 0 && fsync(conversation.journal) == 0);
        conversation_close(&conversation);
        assert(conversation_open(&conversation, path) == FC_CORRUPT);
        conversation_close(&conversation);
        remove_directory(path);
    }
    for (int stage ← 0; stage < 2; stage ← stage + 1) {
        fresh_conversation(&conversation, path);
        feed_bytes(&conversation, (const unsigned char *)"durable before journal", 22);
        uint64_t before ← conversation.journal_end;
        conversation.fail_barrier_after ← stage;
        assert(commit_stored_prefix(&conversation) == FC_STORAGE_ERROR);
        assert(conversation.extents.committed == 0 && conversation.journal_end == before);
        assert(conversation.extents.durable == (stage ? 22u : 0u));
        conversation_close(&conversation);
        remove_directory(path);
    }
    fresh_conversation(&conversation, path);
    conversation.force_reservation_writes ← 1;
    conversation.write_budget ← 3;
    WriteResult exhausted ← store_response_bytes(&conversation, conversation.request, conversation.attempt, "x", 1);
    assert(exhausted.result == FC_STORAGE_ERROR && exhausted.consumed == 0);
    assert(conversation.measurements.reserve_bytes == 3 && conversation.extents.written == 0);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == UNCERTAIN);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS arena corrupt old bytes, torn/stale extents, FC01 refusal, ordered failed barriers and reservation disk exhaustion");
}
static void durability_death_cases(void) {
    for (int stage ← 1; stage <= 4; stage ← stage + 1) {
        char path[] ← "/tmp/fastchat-stages-XXXXXX";
        assert(mkdtemp(path));
        pid_t child ← fork();
        assert(child >= 0);
        if (!child) {
            Conversation conversation;
            if (conversation_open(&conversation, path) != FC_OK || submit_text(&conversation, "crash", 5) != FC_OK
                || admit_event(&conversation, conversation.request, conversation.attempt, START) != FC_OK) _exit(70);
            conversation.crash_at ← stage;
            if (store_response_bytes(&conversation, conversation.request, conversation.attempt, "prefix", 6).result != FC_OK) _exit(71);
            commit_stored_prefix(&conversation);
            _exit(72);
        }
        int status;
        assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 73);
        Conversation replay;
        assert(conversation_open(&replay, path) == FC_OK && replay.phase == UNCERTAIN);
        assert(replay.extents.committed == (stage < 3 ? 0u : 6u));
        assert(retry_request(&replay) == FC_OK);
        assert(replay.response_base == (stage < 3 ? 0u : 6u));
        conversation_close(&replay);
        remove_directory(path);
    }
    puts("PASS fresh-process death after data write, response fsync, journal write and journal fsync; no invented completion");
}
static void pending_decision_case(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, (const unsigned char *)"old", 3);
    finish_conversation(&conversation);
    uint64_t before ← conversation.journal_end;
    assert(submit_text(&conversation, "new", 3) == FC_OK);
    assert(ftruncate(conversation.journal, (off_t)(before + 64 + 3)) == 0);
    assert(fsync(conversation.journal) == 0);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK);
    assert(conversation.request == 2 && conversation.phase == UNCERTAIN && conversation.extents.committed == 0);
    HistoryTurn current, prior;
    assert(history_turn(&conversation, 2, &current) == FC_OK && current.extent == 0);
    assert(history_turn(&conversation, 1, &prior) == FC_OK && prior.extent == 3);
    assert(retry_request(&conversation) == FC_OK);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, START) == FC_OK);
    assert(store_response_bytes(&conversation, conversation.request, conversation.attempt, "\xc0\x80", 2).result == FC_INVALID_TEXT);
    assert(store_response_bytes(&conversation, conversation.request, conversation.attempt, "\xe2", 1).result == FC_OK);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, COMPLETE) == FC_REJECTED);
    assert(store_response_bytes(&conversation, conversation.request, conversation.attempt, "\x82\xac", 2).result == FC_OK);
    finish_conversation(&conversation);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == COMPLETED);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS user decision interrupted before attempt has no prior response; incremental UTF-8 rejects invalid/truncated completion");
}
static void history_case(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, (const unsigned char *)"older", 5);
    finish_conversation(&conversation);
    assert(submit_text(&conversation, "newer prompt", 12) == FC_OK);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, START) == FC_OK);
    feed_bytes(&conversation, (const unsigned char *)"newer", 5);
    finish_conversation(&conversation);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK);
    HistoryTurn older, newer;
    assert(history_turn(&conversation, 1, &older) == FC_OK && history_turn(&conversation, 2, &newer) == FC_OK);
    assert(older.phase == COMPLETED && newer.phase == COMPLETED && older.base+older.extent == newer.base);
    assert(!strcmp(older.prompt, "hello") && !strcmp(newer.prompt, "newer prompt"));
    unsigned char bytes[FC_VIEW_BYTES];
    size_t length;
    assert(read_history_window(&conversation, &older, 0, bytes, sizeof(bytes), &length) == FC_OK && length == 5 && !memcmp(bytes, "older", 5));
    assert(read_history_window(&conversation, &newer, 0, bytes, sizeof(bytes), &length) == FC_OK && length == 5 && !memcmp(bytes, "newer", 5));
    assert(history_turn(&conversation, 3, &newer) == FC_REJECTED);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS bounded durable history navigation preserves both turns after restart");
}
static void production_provider_cases(void) {
    static const char delta[] ← "{\"type\":\"response.output_text.delta\",\"delta\":\"A\\u20ac\\ud83d\\ude42\\n```c\\nx\\n```\"}";
    static const char completion[] ← "{\"type\":\"response.completed\",\"response\":{\"status\":\"completed\"}}";
    for (size_t cut ← 0; cut <= sizeof(delta)-1; cut ← cut + 1) {
        Conversation conversation;
        char path[64];
        fresh_conversation(&conversation, path);
        FixtureTransport transport;
        fixture_transport_open(&transport, &conversation);
        char frame[512];
        int count ← snprintf(frame, sizeof(frame), "data: %s\n\n", delta);
        size_t split ← cut < (size_t)count ? cut : (size_t)count;
        assert(fixture_transport_offer(&transport, frame, split).result == FC_OK);
        assert(fixture_transport_offer(&transport, frame+split, (size_t)count-split).result == FC_OK);
        count ← snprintf(frame, sizeof(frame), "data: %s\n\n", completion);
        for (int byte ← 0; byte < count; byte ← byte + 1) assert(fixture_transport_offer(&transport, frame+byte, 1).result == FC_OK);
        assert(conversation.phase == COMPLETED);
        static const unsigned char expected[] ← "A\xe2\x82\xac\xf0\x9f\x99\x82\n```c\nx\n```";
        expect_bytes(&conversation, expected, sizeof(expected)-1);
        conversation_close(&conversation);
        assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == COMPLETED);
        expect_bytes(&conversation, expected, sizeof(expected)-1);
        conversation_close(&conversation);
        remove_directory(path);
    }
    static const char *bad[] ← {
        "{}", "{\"type\":\"unknown\"}", "{\"type\":\"response.output_text.delta\",\"delta\":3}",
        "{\"type\":\"response.output_text.delta\",\"delta\":\"\\ud800\"}",
        "{\"type\":\"response.output_text.delta\",\"delta\":\"a\",\"delta\":\"b\"}",
        "{\"type\":\"response.output_text.delta\",\"delta\":\"a\",\"\\u0064elta\":\"b\"}",
        "{\"type\":\"response.completed\",\"response\":{\"status\":\"failed\"}}",
        "{\"choices\":[{\"delta\":{\"content\":\"x\"}}]}garbage", "{\"choices\":[{\"delta\":{}} ,]}",
        "{\"choices\":[{\"delta\":{},\"n\":01}]}", "{\"choices\":[{\"delta\":{},\"n\":1.}]}"
    };
    for (size_t index ← 0; index < sizeof(bad)/sizeof(bad[0]); index ← index + 1) {
        Conversation conversation;
        char path[64];
        fresh_conversation(&conversation, path);
        Provider provider;
        provider_open(&provider, &conversation);
        assert(provider_record(&provider, bad[index], strlen(bad[index])) == FC_INVALID_TEXT);
        assert(conversation.phase != COMPLETED && conversation.extents.written == 0);
        conversation_close(&conversation);
        remove_directory(path);
    }
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    Provider provider;
    provider_open(&provider, &conversation);
    conversation.would_block ← 1;
    assert(provider_record(&provider, delta, sizeof(delta)-1) == FC_BACKPRESSURE);
    assert(provider.length > 0 && provider.written == 0);
    assert(provider_record(&provider, delta, sizeof(delta)-1) == FC_BACKPRESSURE);
    conversation.would_block ← 0;
    assert(provider_resume(&provider) == FC_OK && provider.length == 0);
    static const char tool[] ← "{\"type\":\"response.function_call_arguments.delta\",\"delta\":\"{}\",\"item_id\":\"fixture-item\"}";
    uint64_t before ← conversation.sequence;
    assert(provider_record(&provider, tool, sizeof(tool)-1) == FC_OK && conversation.sequence == before+2); /* prefix then tool */
    static const char chat[] ← "{\"choices\":[{\"delta\":{\"content\":\"chat\",\"tool_calls\":[]},\"finish_reason\":null}]}";
    assert(provider_record(&provider, chat, sizeof(chat)-1) == FC_OK);
    static const char failure[] ← "{\"type\":\"error\",\"message\":\"fixture provider failure\"}";
    assert(provider_record(&provider, failure, sizeof(failure)-1) == FC_OK && conversation.phase == FAILED);
    assert(provider_record(&provider, delta, sizeof(delta)-1) == FC_REJECTED);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == FAILED);
    conversation_close(&conversation);
    remove_directory(path);
    char body[8192];
    size_t length;
    assert(provider_request_body("fixture-model", "quote\"\n\\", 8, body, sizeof(body), &length) == FC_OK);
    assert(strstr(body, "quote\\\"\\u000a\\\\"));
    assert(provider_request_body("bad\"model", "x", 1, body, sizeof(body), &length) == FC_REJECTED);
    puts("PASS production-shaped SSE/JSON cuts, UTF-8/fences, ordinary text, explicit failure, malformed/truncated data, durable tool events and bounded backpressure");
}

static void cancellation_cases(void) {
    Conversation pending;
    char pending_path[] ← "/tmp/fastchat-cancel-XXXXXX";
    assert(mkdtemp(pending_path));
    assert(conversation_open(&pending, pending_path) == FC_OK);
    assert(submit_text(&pending, "hello", 5) == FC_OK);
    assert(admit_event(&pending, pending.request, pending.attempt, CANCEL_REQUEST) == FC_OK);
    feed_bytes(&pending, (const unsigned char *)"pending", 7);
    assert(admit_event(&pending, pending.request, pending.attempt, START) == FC_OK);
    finish_conversation(&pending);
    conversation_close(&pending);
    assert(conversation_open(&pending, pending_path) == FC_OK && pending.phase == COMPLETED);
    conversation_close(&pending);
    remove_directory(pending_path);

    for (int cancellation_wins ← 0; cancellation_wins < 2; cancellation_wins ← cancellation_wins + 1) {
        Conversation conversation;
        char path[64];
        fresh_conversation(&conversation, path);
        feed_bytes(&conversation, (const unsigned char *)"a", 1);
        assert(admit_event(&conversation, conversation.request, conversation.attempt, CANCEL_REQUEST) == FC_OK);
        feed_bytes(&conversation, (const unsigned char *)"b", 1);
        Event first ← cancellation_wins ? CANCEL_ACK : COMPLETE;
        Event late ← cancellation_wins ? COMPLETE : CANCEL_ACK;
        assert(admit_event(&conversation, conversation.request, conversation.attempt, first) == FC_OK);
        uint64_t sequence ← conversation.sequence;
        assert(admit_event(&conversation, conversation.request, conversation.attempt, late) == FC_REJECTED);
        assert(conversation.sequence == sequence);
        conversation_close(&conversation);
        assert(conversation_open(&conversation, path) == FC_OK);
        assert(conversation.phase == (cancellation_wins ? CANCELLED : COMPLETED));
        conversation_close(&conversation);
        remove_directory(path);
    }
    Conversation failure;
    char path[64];
    fresh_conversation(&failure, path);
    assert(admit_event(&failure, failure.request, failure.attempt, FAILURE) == FC_OK);
    assert(failure.phase == FAILED);
    assert(store_response_bytes(&failure, failure.request, failure.attempt, "late", 4).result == FC_REJECTED);
    conversation_close(&failure);
    assert(conversation_open(&failure, path) == FC_OK && failure.phase == FAILED);
    conversation_close(&failure);
    remove_directory(path);
    puts("PASS pending cancellation, chunks after cancel request, both terminal race orders, explicit failure");
}
static void interrupted_cases(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    uint64_t initial_events ← conversation.sequence;
    conversation.short_write ← 1;
    feed_bytes(&conversation, (const unsigned char *)"abc", 3);
    assert(conversation.extents.written == 3 && conversation.extents.durable == 0
           && conversation.extents.committed == 0);
    conversation.write_budget ← 2;
    WriteResult partial ← store_response_bytes(&conversation, conversation.request,
                                               conversation.attempt, "defg", 4);
    assert(partial.result == FC_STORAGE_ERROR && partial.consumed == 2);
    assert(conversation.extents.written == 5 && conversation.sequence == initial_events);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, COMPLETE) == FC_STORAGE_ERROR);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK);
    assert(conversation.phase == UNCERTAIN && conversation.recovered_partial);
    assert(conversation.extents.committed == 0);
    uint64_t old_attempt ← conversation.attempt;
    uint64_t old_request ← conversation.request;
    assert(retry_request(&conversation) == FC_OK);
    assert(conversation.request == old_request && conversation.attempt != old_attempt);
    uint64_t sequence ← conversation.sequence;
    assert(admit_event(&conversation, old_request, old_attempt, COMPLETE) == FC_REJECTED);
    assert(store_response_bytes(&conversation, old_request, old_attempt, "late", 4).result == FC_REJECTED);
    assert(conversation.sequence == sequence);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, START) == FC_OK);
    feed_bytes(&conversation, (const unsigned char *)"replacement", 11);
    finish_conversation(&conversation);
    expect_bytes(&conversation, (const unsigned char *)"replacement", 11);
    conversation_close(&conversation);
    remove_directory(path);

    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, (const unsigned char *)"partial", 7);
    assert(admit_event(&conversation, conversation.request, conversation.attempt, TRANSPORT_LOSS) == FC_OK);
    assert(conversation.phase == UNCERTAIN && conversation.extents.committed == 7);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == UNCERTAIN);
    assert(conversation.extents.committed == 7);
    conversation_close(&conversation);
    remove_directory(path);

    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, (const unsigned char *)"x", 1);
    conversation.fail_barrier ← 1;
    assert(commit_stored_prefix(&conversation) == FC_STORAGE_ERROR);
    assert(conversation.extents.durable == 0 && conversation.extents.committed == 0);
    conversation_close(&conversation);
    remove_directory(path);
    fresh_conversation(&conversation, path);
    conversation.would_block ← 1;
    WriteResult blocked ← store_response_bytes(&conversation, conversation.request, conversation.attempt, "pending", 7);
    assert(blocked.result == FC_BACKPRESSURE && blocked.consumed == 0);
    assert(conversation.extents.received == 7 && conversation.extents.written == 0);
    uint64_t before_complete ← conversation.sequence;
    assert(admit_event(&conversation, conversation.request, conversation.attempt, COMPLETE) == FC_REJECTED);
    assert(conversation.sequence == before_complete && conversation.phase == GENERATING);
    conversation.would_block ← 0;
    feed_bytes(&conversation, (const unsigned char *)"pending", 7);
    finish_conversation(&conversation);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == COMPLETED);
    assert(conversation.extents.received == 7 && conversation.extents.written == 7);
    expect_bytes(&conversation, (const unsigned char *)"pending", 7);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS partial/short writes, exhaustion, failed barrier, interrupted replay, uncertainty, fresh retry, stale attempt, finish refuses unwritten received bytes");
}
static void process_death_case(void) {
    char path[] ← "/tmp/fastchat-death-XXXXXX";
    assert(mkdtemp(path));
    pid_t child ← fork();
    assert(child >= 0);
    if (!child) {
        Conversation conversation;
        if (conversation_open(&conversation, path) != FC_OK
            || submit_text(&conversation, "hello", 5) != FC_OK
            || admit_event(&conversation, conversation.request, conversation.attempt, START) != FC_OK)
            _exit(10);
        unsigned char bytes[FC_BATCH_BYTES];
        memset(bytes, 'z', sizeof(bytes));
        WriteResult written ← store_response_bytes(&conversation, conversation.request,
                                                   conversation.attempt, bytes, sizeof(bytes));
        if (written.result != FC_OK) _exit(11);
        written ← store_response_bytes(&conversation, conversation.request, conversation.attempt, "tail", 4);
        _exit(written.result == FC_OK ? 0 : 12);
    }
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    Conversation recovered;
    assert(conversation_open(&recovered, path) == FC_OK);
    assert(recovered.phase == UNCERTAIN && recovered.recovered_partial);
    assert(recovered.extents.committed == FC_BATCH_BYTES);
    conversation_close(&recovered);
    remove_directory(path);
    puts("PASS process death: committed prefix recoverable, uncommitted tail not completed");
}
static void journal_cases(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, (const unsigned char *)"final", 5);
    finish_conversation(&conversation);
    uint64_t sequence ← conversation.sequence;
    uint64_t journal_end ← conversation.journal_end;
    assert(pwrite(conversation.journal, "FC01", 4, (off_t)journal_end) == 4);
    assert(fsync(conversation.journal) == 0);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK);
    assert(conversation.phase == COMPLETED && conversation.sequence == sequence);
    struct stat status;
    assert(fstat(conversation.journal, &status) == 0 && (uint64_t)status.st_size == journal_end);
    conversation_close(&conversation);
    remove_directory(path);

    fresh_conversation(&conversation, path);
    conversation.write_budget ← 5;
    assert(admit_event(&conversation, conversation.request, conversation.attempt, FAILURE) == FC_STORAGE_ERROR);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_OK);
    assert(conversation.phase == UNCERTAIN);
    conversation_close(&conversation);
    remove_directory(path);

    fresh_conversation(&conversation, path);
    feed_bytes(&conversation, (const unsigned char *)"body", 4);
    finish_conversation(&conversation);
    unsigned char byte ← 0;
    assert(pwrite(conversation.journal, &byte, 1, 16) == 1);
    assert(fsync(conversation.journal) == 0);
    conversation_close(&conversation);
    assert(conversation_open(&conversation, path) == FC_CORRUPT);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS torn journal tail, interrupted terminal record, complete-record corruption rejected");
}
static void bounded_cases(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    conversation.would_block ← 1;
    WriteResult blocked ← store_response_bytes(&conversation, conversation.request,
                                               conversation.attempt, "abc", 3);
    assert(blocked.result == FC_BACKPRESSURE && blocked.consumed == 0);
    assert(conversation.extents.written == 0);
    conversation.would_block ← 0;
    feed_bytes(&conversation, (const unsigned char *)"abc", 3);
    size_t giant_length ← 1024 * 1024;
    unsigned char *giant ← malloc(giant_length);
    assert(giant);
    memset(giant, 'm', giant_length);
    feed_bytes(&conversation, giant, giant_length);
    assert(conversation.measurements.maximum_write <= FC_WRITE_BYTES);
    finish_conversation(&conversation);
    unsigned char window[FC_VIEW_BYTES];
    size_t length;
    assert(read_response_window(&conversation, 0, 0, window, sizeof(window), &length) == FC_OK);
    assert(length == FC_VIEW_BYTES && conversation.measurements.maximum_view == FC_VIEW_BYTES);
    free(giant);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS explicit backpressure, giant offered chunk, bounded writes/view, slow renderer retains no queue");
}
static void ram_control_case(void) {
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    RamControl control ← {0};
    unsigned char bytes[257];
    for (size_t index ← 0; index < sizeof(bytes); index ← index + 1) bytes[index] ← (unsigned char)('a' + index % 26);
    for (size_t chunk ← 0; chunk < 333; chunk ← chunk + 1) {
        size_t length ← chunk % sizeof(bytes) + 1;
        assert(ram_write(&control, bytes, length));
        feed_bytes(&conversation, bytes, length);
    }
    ram_finish(&control);
    finish_conversation(&conversation);
    expect_bytes(&conversation, control.bytes, control.length);
    assert(!ram_write(&control, bytes, 1));
    free(control.bytes);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS RAM control and disk exact completed logical response, sealed sinks");
}

static long resident_kib(void) {
    FILE *status ← fopen("/proc/self/statm", "r");
    unsigned long pages, resident;
    if (!status) return -1;
    int fields ← fscanf(status, "%lu %lu", &pages, &resident);
    fclose(status);
    if (fields != 2) return -1;
    return (long)(resident * (unsigned long)sysconf(_SC_PAGESIZE) / 1024);
}
/* VmHWM belongs to this executable's memory image. ru_maxrss can retain the
   launcher's pre-exec high water mark and obscure this small control. */
static long image_peak_kib(void) {
    FILE *status ← fopen("/proc/self/status", "r");
    if (!status) return -1;
    char line[256];
    long peak ← -1;
    while (fgets(line, sizeof(line), status)) {
        if (sscanf(line, "VmHWM: %ld kB", &peak) == 1) break;
    }
    fclose(status);
    return peak;
}
static int benchmark_response(const char *mode, const char *path) {
    assert(policy_open() == FC_OK); /* Same bounded runtime in the RAM control. */
    int ram ← strcmp(mode, "ram") == 0;
    int streaming ← strcmp(mode, "streaming") == 0;
    if (!ram && streaming != renderer_follows_prefixes()) {
        fprintf(stderr, "benchmark mode must match this branch's renderer policy\n");
        return 2;
    }
    Conversation conversation;
    RamControl control ← {0};
    if (!ram) {
        assert(conversation_open(&conversation, path) == FC_OK);
        assert(conversation.phase == IDLE);
        assert(submit_text(&conversation, "benchmark", 9) == FC_OK);
        assert(admit_event(&conversation, conversation.request, conversation.attempt, START) == FC_OK);
    }
    unsigned char offered[FC_WRITE_BYTES];
    memset(offered, 'a', sizeof(offered));
    unsigned char viewport[FC_VIEW_BYTES];
    size_t count;
    uint64_t shown ← 0;
    uint32_t displayed_crc ← 0xffffffffu;
    double began ← clock_seconds(CLOCK_MONOTONIC);
    double cpu_began ← clock_seconds(CLOCK_PROCESS_CPUTIME_ID);
    double first_visible ← -1;
    for (size_t fragment ← 0; fragment < 2048; fragment ← fragment + 1) {
        if (ram) assert(ram_write(&control, offered, sizeof(offered)));
        else {
            feed_bytes(&conversation, offered, sizeof(offered));
            if (streaming) {
                assert(render_stored_window(&conversation, shown, viewport, sizeof(viewport), &count) == FC_OK);
                if (count) {
                    if (first_visible < 0) first_visible ← clock_seconds(CLOCK_MONOTONIC) - began;
                    displayed_crc ← checksum_bytes(displayed_crc, viewport, count);
                    shown ← shown + count;
                }
            }
        }
    }
    double generating_cpu ← clock_seconds(CLOCK_PROCESS_CPUTIME_ID) - cpu_began;
    long steady_rss ← resident_kib();
    double terminal ← clock_seconds(CLOCK_MONOTONIC);
    if (ram) ram_finish(&control);
    else finish_conversation(&conversation);
    double first_final ← -1;
    while (shown < 8 * 1024 * 1024) {
        if (ram) {
            count ← control.length - shown > sizeof(viewport) ? sizeof(viewport) : (size_t)(control.length - shown);
            memcpy(viewport, control.bytes + shown, count);
        } else assert(render_stored_window(&conversation, shown, viewport, sizeof(viewport), &count) == FC_OK);
        assert(count);
        double now ← clock_seconds(CLOCK_MONOTONIC);
        if (first_visible < 0) first_visible ← now - began;
        if (first_final < 0) first_final ← now - terminal;
        displayed_crc ← checksum_bytes(displayed_crc, viewport, count);
        shown ← shown + count;
    }
    double drain_latency ← clock_seconds(CLOCK_MONOTONIC) - terminal;
    long peak_rss ← image_peak_kib();
    if (ram) {
        printf("mode\tpeak_rss_kib\tsteady_rss_kib\tresponse_bytes\tdata_bytes\tjournal_bytes\twrites\tmax_write\tmax_view\tfirst_text_ms\tterminal_first_window_ms\tterminal_drain_ms\tgeneration_cpu_ms\treplay_ms\tresponse_crc\n");
        printf("ram\t%ld\t%ld\t%zu\t0\t0\t0\t0\t%u\t%.3f\t%.3f\t%.3f\t%.3f\tNOT_APPLICABLE\t%08x\n",
               peak_rss, steady_rss, control.length, FC_VIEW_BYTES, first_visible * 1000,
               first_final * 1000, drain_latency * 1000, generating_cpu * 1000, displayed_crc);
        free(control.bytes);
    } else {
        StoreMeasurements measurements ← conversation.measurements;
        assert(displayed_crc == conversation.response_crc);
        conversation_close(&conversation);
        double replay_began ← clock_seconds(CLOCK_MONOTONIC);
        assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == COMPLETED);
        double replay_time ← clock_seconds(CLOCK_MONOTONIC) - replay_began;
        assert(conversation.response_crc == displayed_crc && conversation.extents.committed == shown);
        conversation_close(&conversation);
        printf("mode\tpeak_rss_kib\tsteady_rss_kib\tresponse_bytes\tdata_bytes\tjournal_bytes\twrites\tmax_write\tmax_view\tfirst_text_ms\tterminal_first_window_ms\tterminal_drain_ms\tgeneration_cpu_ms\treplay_ms\tresponse_crc\n");
        printf("%s\t%ld\t%ld\t%llu\t%llu\t%llu\t%llu\t%zu\t%zu\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%08x\n",
               mode, peak_rss, steady_rss, (unsigned long long)shown,
               (unsigned long long)measurements.data_bytes, (unsigned long long)measurements.journal_bytes,
               (unsigned long long)measurements.write_count, measurements.maximum_write, measurements.maximum_view,
               first_visible * 1000, first_final * 1000, drain_latency * 1000,
               generating_cpu * 1000, replay_time * 1000, displayed_crc);
        printf("allocation_bytes\t%llu\nallocation_calls\t%llu\nfallback_reserve_bytes\t%llu\nfallback_reserve_writes\t%llu\n",
               (unsigned long long)measurements.allocation_bytes,
               (unsigned long long)measurements.allocation_calls,
               (unsigned long long)measurements.reserve_bytes,
               (unsigned long long)measurements.reserve_writes);
    }
    return 0;
}

static int fresh_process_replay(const char *path) {
    Conversation conversation;
    double began ← clock_seconds(CLOCK_MONOTONIC);
    assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == COMPLETED);
    double ready ← clock_seconds(CLOCK_MONOTONIC);
    unsigned char viewport[FC_VIEW_BYTES];
    size_t count;
    assert(render_stored_window(&conversation, 0, viewport, sizeof(viewport), &count) == FC_OK);
    printf("replay_context\tfresh_process_filesystem_cache_unspecified\n");
    printf("replay_ms\t%.3f\nfirst_window_ms\t%.3f\nbytes\t%llu\ncrc\t%08x\n",
           (ready - began) * 1000, (clock_seconds(CLOCK_MONOTONIC) - began) * 1000,
           (unsigned long long)conversation.extents.committed, conversation.response_crc);
    assert(count == FC_VIEW_BYTES);
    conversation_close(&conversation);
    return 0;
}
/* Keep these stores for byte-for-byte sibling comparison. All callers use
   the branch renderer policy; provider split sizes do not change history. */
static int comparison_corpus(const char *root) {
    assert(mkdir(root, 0700) == 0 || errno == EEXIST);
    static const char *frames[] ← {
        "data: [START]\n\ndata: [DONE]\n\n",
        "data: [START]\n\ndata: {\"text\":\"one chunk\"}\n\ndata: [DONE]\n\n",
        "data: [START]\n\ndata: {\"text\":\"A\"}\n\ndata: {\"text\":\"\\u20ac\\ud83d\\ude42\"}\n\ndata: {\"text\":\"**bold**\\n```\\nx\\n```\\n\"}\n\ndata: [DONE]\n\n"
    };
    static const size_t splits[] ← {1, 2, 7, 257, 8192};
    for (size_t fixture ← 0; fixture < 3; fixture ← fixture + 1) {
        for (size_t split ← 0; split < sizeof(splits)/sizeof(splits[0]); split ← split + 1) {
            char path[4096];
            int count ← snprintf(path, sizeof(path), "%s/fixture-%zu-split-%zu", root, fixture, splits[split]);
            assert(count > 0 && (size_t)count < sizeof(path));
            Conversation conversation;
            assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == IDLE);
            assert(submit_text(&conversation, "same corpus", 11) == FC_OK);
            FixtureTransport transport;
            fixture_transport_open(&transport, &conversation);
            size_t position ← 0, length ← strlen(frames[fixture]);
            uint64_t visible ← 0;
            while (position < length) {
                size_t amount ← length-position < splits[split] ? length-position : splits[split];
                WriteResult offered ← fixture_transport_offer(&transport, frames[fixture]+position, amount);
                assert(offered.result == FC_OK && offered.consumed == amount);
                position ← position + offered.consumed;
                unsigned char window[FC_VIEW_BYTES];
                size_t shown;
                assert(render_stored_window(&conversation, visible, window, sizeof(window), &shown) == FC_OK);
                visible ← visible + shown;
            }
            assert(conversation.phase == COMPLETED);
            conversation_close(&conversation);
            assert(conversation_open(&conversation, path) == FC_OK && conversation.phase == COMPLETED);
            printf("corpus\tfixture=%zu\tsplit=%zu\tbytes=%llu\tevents=%llu\tcrc=%08x\treplay=completed\n",
                fixture, splits[split], (unsigned long long)conversation.extents.committed,
                (unsigned long long)conversation.sequence, conversation.response_crc);
            conversation_close(&conversation);
        }
    }
    return 0;
}

static void framing_cases(void) {
    static const char stream[] ←
        ": keepalive\r\n\r\ndata: [START]\r\n\r\ndata:\r\n\r\n"
        "data: {\"text\":\"A\\u20ac\\ud83d\\ude42\\n**bold**\\n```\\nx\\n```\\n\"}\r\n\r\n"
        "data: [DONE]\r\n\r\n";
    static const unsigned char expected[] ← "A\xe2\x82\xac\xf0\x9f\x99\x82\n**bold**\n```\nx\n```\n";
    for (size_t split ← 0; split < sizeof(stream); split ← split + 1) {
        Conversation conversation;
        char path[64];
        strcpy(path, "/tmp/fastchat-test-XXXXXX");
        assert(mkdtemp(path));
        assert(conversation_open(&conversation, path) == FC_OK);
        assert(submit_text(&conversation, "framing", 7) == FC_OK);
        FixtureTransport transport;
        fixture_transport_open(&transport, &conversation);
        WriteResult first ← fixture_transport_offer(&transport, stream, split);
        assert(first.result == FC_OK && first.consumed == split);
        WriteResult rest ← fixture_transport_offer(&transport, stream + split, sizeof(stream) - 1 - split);
        assert(rest.result == FC_OK && rest.consumed == sizeof(stream) - 1 - split);
        assert(transport.finished && conversation.phase == COMPLETED);
        expect_bytes(&conversation, expected, sizeof(expected) - 1);
        assert(transport.maximum_buffered <= FC_FRAME_BYTES);
        conversation_close(&conversation);
        remove_directory(path);
    }
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    FixtureTransport transport;
    fixture_transport_open(&transport, &conversation);
    conversation.would_block ← 1;
    static const char frame[] ← "data: {\"text\":\"retained once\"}\n\n";
    WriteResult blocked ← fixture_transport_offer(&transport, frame, sizeof(frame) - 1);
    assert(blocked.result == FC_BACKPRESSURE && blocked.consumed == sizeof(frame) - 1);
    assert(transport.pending && conversation.extents.written == 0);
    conversation.would_block ← 0;
    WriteResult resumed ← fixture_transport_offer(&transport, "", 0);
    assert(resumed.result == FC_OK && !transport.pending);
    finish_conversation(&conversation);
    expect_bytes(&conversation, (const unsigned char *)"retained once", 13);
    conversation_close(&conversation);
    remove_directory(path);
    fresh_conversation(&conversation, path);
    fixture_transport_open(&transport, &conversation);
    assert(fixture_transport_offer(&transport, "data: {\"text\":\"unfinished", 25).result == FC_OK);
    assert(fixture_transport_lost(&transport) == FC_OK && conversation.phase == UNCERTAIN);
    assert(conversation.extents.committed == 0);
    conversation_close(&conversation);
    remove_directory(path);
    fresh_conversation(&conversation, path);
    fixture_transport_open(&transport, &conversation);
    static const char invalid[] ← "data: {\"text\":\"\\ud800x\"}\n\n";
    assert(fixture_transport_offer(&transport, invalid, sizeof(invalid) - 1).result == FC_INVALID_TEXT);
    assert(conversation.extents.written == 0 && conversation.phase != COMPLETED);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS every SSE/JSON split, CRLF, keepalives, surrogate pair, Markdown/fence bytes, framed backpressure, truncated loss, malformed JSON rejection");
}

static void streaming_text_cases(void) {
    static const unsigned char text[] ← "A\xe2\x82\xac\xf0\x9f\x99\x82\n**bold**\n```\nx\n```\n";
    Conversation conversation;
    char path[64];
    fresh_conversation(&conversation, path);
    unsigned char shown[FC_VIEW_BYTES];
    size_t displayed ← 0;
    for (size_t byte ← 0; byte < sizeof(text) - 1; byte ← byte + 1) {
        feed_bytes(&conversation, text + byte, 1);
        /* Deliberately force a durability boundary inside every code point.
           This is a hostile test, not the production batching policy. */
        assert(commit_stored_prefix(&conversation) == FC_OK);
        size_t count;
        assert(read_response_window(&conversation, displayed, 1, shown, sizeof(shown), &count) == FC_OK);
        int valid;
        assert(complete_utf8_prefix(shown, count, &valid) == count && valid);
        assert(!memcmp(shown, text + displayed, count));
        displayed ← displayed + count;
        assert(displayed <= conversation.extents.committed);
    }
    assert(displayed == sizeof(text) - 1);
    finish_conversation(&conversation);
    expect_bytes(&conversation, text, sizeof(text) - 1);
    conversation_close(&conversation);
    remove_directory(path);
    puts("PASS streaming stored prefixes with incomplete UTF-8 withheld, tiny chunks, delimiters/fences preserved");
}

static void lua_policy_cases(void) {
    int expected_renderer ← renderer_follows_prefixes();
    assert(expected_renderer == 0 || expected_renderer == 1);
    FixtureScenario scenario;
    assert(policy_fixture_scenario(":long", 5, 0, &scenario) == FC_OK && scenario == FIXTURE_LONG);
    assert(policy_fixture_scenario(":lost", 5, 0, &scenario) == FC_OK && scenario == FIXTURE_LOST);
    assert(policy_fixture_scenario(":lost", 5, 1, &scenario) == FC_OK && scenario == FIXTURE_SHORT);
    assert(policy_fixture_scenario(":fail", 5, 0, &scenario) == FC_OK && scenario == FIXTURE_FAILED);
    assert(policy_fixture_scenario("a normal message", 16, 0, &scenario) == FC_OK && scenario == FIXTURE_SHORT);
    ComposerAction action;
    for (Phase phase ← IDLE; phase <= FAILED; phase ← phase + 1) {
        assert(policy_composer_action(phase, 1, &action) == FC_OK && action == POLICY_CANCEL);
        assert(policy_composer_action(phase, 0, &action) == FC_OK);
        assert(action == (phase == UNCERTAIN ? POLICY_RETRY :
            phase == IDLE || phase == COMPLETED || phase == CANCELLED || phase == FAILED ? POLICY_SUBMIT : POLICY_WAIT));
        int due;
        assert(policy_barrier_due(phase, 199999999u, 1, &due) == FC_OK && !due);
        assert(policy_barrier_due(phase, 200000000u, 1, &due) == FC_OK);
        assert(due == (phase == GENERATING || phase == CANCEL_PENDING));
        assert(policy_barrier_due(phase, UINT64_MAX, 0, &due) == FC_OK && !due);
    }
    Conversation conversation;
    char path[64];
    strcpy(path, "/tmp/fastchat-test-XXXXXX");
    assert(mkdtemp(path) && conversation_open(&conversation, path) == FC_OK);
    assert(submit_text(&conversation, "Lua fixture", 11) == FC_OK);
    FixtureTransport transport;
    fixture_transport_open(&transport, &conversation);
    for (unsigned step ← 0; step <= 4; step ← step + 1) {
        FixturePlan plan;
        assert(policy_fixture_frame(step, 0, &plan) == FC_OK && plan.terminal == (step == 4));
        /* Each actual policy frame is deliberately split to one-byte offers. */
        for (size_t position ← 0; position < plan.length; position ← position + 1) {
            WriteResult offered ← fixture_transport_offer(&transport, plan.bytes + position, 1);
            assert(offered.result == FC_OK && offered.consumed == 1);
        }
    }
    static const unsigned char expected[] ←
        "A local response, stored before presentation.\n\n"
        "UTF-8: \xe2\x82\xac \xf0\x9f\x99\x82. Markdown is plain readable text:\n"
        "**disk authority**\n```text\nbounded windows\n```\n";
    assert(conversation.phase == COMPLETED);
    expect_bytes(&conversation, expected, sizeof(expected)-1);
    conversation_close(&conversation);
    remove_directory(path);
    for (FixtureScenario outcome ← FIXTURE_LOST; outcome <= FIXTURE_FAILED; outcome ← outcome + 1) {
        strcpy(path, "/tmp/fastchat-test-XXXXXX");
        assert(mkdtemp(path) && conversation_open(&conversation, path) == FC_OK);
        assert(submit_text(&conversation, "terminal fixture", 16) == FC_OK);
        fixture_transport_open(&transport, &conversation);
        for (unsigned step ← 0; step <= 4; step ← step + 1) {
            FixturePlan plan;
            assert(policy_fixture_frame(step, outcome, &plan) == FC_OK);
            WriteResult offered ← fixture_transport_offer(&transport, plan.bytes, plan.length);
            assert(offered.result == FC_OK && offered.consumed == plan.length);
        }
        assert(conversation.phase == (outcome == FIXTURE_LOST ? UNCERTAIN : FAILED));
        conversation_close(&conversation);
        assert(conversation_open(&conversation, path) == FC_OK);
        assert(conversation.phase == (outcome == FIXTURE_LOST ? UNCERTAIN : FAILED));
        conversation_close(&conversation);
        remove_directory(path);
    }
    for (unsigned step ← 0; step <= 2049; step ← step + 1) {
        FixturePlan plan;
        assert(policy_fixture_frame(step, 1, &plan) == FC_OK && plan.length <= FC_POLICY_FRAME_BYTES);
        assert(plan.terminal == (step == 2049));
        if (step > 0 && step < 2049) assert(plan.length == 4096 + 19);
    }
    printf("PASS actual Icky Lua composition, composer states, batched barriers, completion/loss/failure fixtures; policy_heap_peak=%zu limit=%u\n",
        policy_memory_peak(), FC_POLICY_BYTES);
    assert(policy_memory_peak() <= FC_POLICY_BYTES);
    assert(policy_load_source("local unfinished ←", strlen("local unfinished ←")) == FC_POLICY_ERROR);
    assert(renderer_follows_prefixes() == -1); /* no automatic fallback */
    policy_close();
    assert(policy_open() == FC_OK && renderer_follows_prefixes() == expected_renderer);
    const char *loop ← "while true do end";
    assert(policy_load_source(loop, strlen(loop)) == FC_POLICY_ERROR);
    policy_close();
    unsigned char oversized[FC_POLICY_BYTES];
    memset(oversized, 'a', sizeof(oversized));
    memcpy(oversized, "return \"", 8);
    oversized[sizeof(oversized)-1] ← '"';
    assert(policy_load_source(oversized, sizeof(oversized)) == FC_POLICY_ERROR);
    assert(policy_memory_denials() && policy_memory_peak() <= FC_POLICY_BYTES);
    policy_close();
    assert(policy_open() == FC_OK && renderer_follows_prefixes() == expected_renderer);
    FixturePlan invalid;
    assert(policy_fixture_frame(2050, 1, &invalid) == FC_POLICY_ERROR);
    assert(renderer_follows_prefixes() == -1);
    policy_close();
    assert(policy_open() == FC_OK && renderer_follows_prefixes() == expected_renderer);
    puts("PASS malformed policy, instruction/heap bounds and invalid result fail closed; explicit reinitialization");
}

static void giant_framed_offer_case(void) {
    Conversation conversation;
    char path[64];
    strcpy(path, "/tmp/fastchat-test-XXXXXX");
    assert(mkdtemp(path) && conversation_open(&conversation, path) == FC_OK);
    assert(submit_text(&conversation, "giant wire offer", 16) == FC_OK);
    FixtureTransport transport;
    fixture_transport_open(&transport, &conversation);
    /* Provider-owned test input. The adapter never copies this entire offer. */
    size_t capacity ← 256 * (4096 + 19) + 64;
    unsigned char *wire ← malloc(capacity);
    assert(wire);
    size_t length ← 0;
    for (unsigned step ← 0; step <= 257; step ← step + 1) {
        FixturePlan plan;
        assert(policy_fixture_frame(step == 257 ? 2049 : step, 1, &plan) == FC_OK);
        assert(plan.length <= capacity - length);
        memcpy(wire + length, plan.bytes, plan.length);
        length ← length + plan.length;
    }
    WriteResult offered ← fixture_transport_offer(&transport, wire, length);
    assert(offered.result == FC_OK && offered.consumed == length && conversation.phase == COMPLETED);
    assert(conversation.extents.committed == 256 * FC_WRITE_BYTES);
    assert(transport.maximum_buffered <= 2 * FC_FRAME_BYTES);
    printf("PASS giant framed input offer=%zu logical_bytes=%llu active_framing_peak=%zu fixed_framing_capacity=%u renderer_queue_bytes=0\n",
        length, (unsigned long long)conversation.extents.committed, transport.maximum_buffered, 2 * FC_FRAME_BYTES);
    unsigned char window[FC_VIEW_BYTES];
    uint64_t offset ← 0;
    while (offset < conversation.extents.committed) {
        size_t count;
        assert(render_stored_window(&conversation, offset, window, sizeof(window), &count) == FC_OK && count);
        for (size_t index ← 0; index < count; index ← index + 1) assert(window[index] == 'a');
        offset ← offset + count;
    }
    free(wire);
    conversation_close(&conversation);
    remove_directory(path);
    fresh_conversation(&conversation, path);
    fixture_transport_open(&transport, &conversation);
    unsigned char oversized[FC_FRAME_BYTES + 100];
    memset(oversized, 'a', sizeof(oversized));
    memcpy(oversized, "data: {\"text\":\"", 15);
    assert(fixture_transport_offer(&transport, oversized, sizeof(oversized)).result == FC_INVALID_TEXT);
    assert(conversation.extents.written == 0 && conversation.phase != COMPLETED);
    conversation_close(&conversation);
    remove_directory(path);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--arena-reuse") == 0) { arena_reuse_case(); return 0; }
    if (argc == 3 && strcmp(argv[1], "replay") == 0) return fresh_process_replay(argv[2]);
    if (argc == 3 && strcmp(argv[1], "corpus") == 0) return comparison_corpus(argv[2]);
    if (argc == 4 && strcmp(argv[1], "benchmark") == 0)
        return benchmark_response(argv[2], argv[3]);

    lua_policy_cases();
    native_retry_order_case();
    arena_hostile_cases();
    durability_death_cases();
    history_case();
    pending_decision_case();
    production_provider_cases();
    giant_framed_offer_case();
    prefix_visibility_case();
    response_cases();
    arena_reuse_case();
    cancellation_cases();
    interrupted_cases();
    process_death_case();
    journal_cases();
    bounded_cases();
    ram_control_case();
    framing_cases();
    streaming_text_cases();
    puts("PASS C host storage/transport core; Android presentation is a separate gate");
    return 0;
}
