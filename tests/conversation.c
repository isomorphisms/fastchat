#define _GNU_SOURCE
#include "conversation.h"
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
static void cancellation_cases(void) {
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
    puts("PASS partial/short writes, exhaustion, failed barrier, interrupted replay, uncertainty, fresh retry, stale attempt");
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

static int benchmark_response(const char *mode, const char *path) {
    int ram ← strcmp(mode, "ram") == 0;
    int streaming ← strcmp(mode, "streaming") == 0;
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
                assert(read_response_window(&conversation, shown, 1, viewport, sizeof(viewport), &count) == FC_OK);
                if (count) {
                    if (first_visible < 0) first_visible ← clock_seconds(CLOCK_MONOTONIC) - began;
                    displayed_crc ← checksum_bytes(displayed_crc, viewport, count);
                    shown ← shown + count;
                }
            }
        }
    }
    double generating_cpu ← clock_seconds(CLOCK_PROCESS_CPUTIME_ID) - cpu_began;
    double terminal ← clock_seconds(CLOCK_MONOTONIC);
    if (ram) ram_finish(&control);
    else finish_conversation(&conversation);
    double first_final ← -1;
    while (shown < 8 * 1024 * 1024) {
        if (ram) {
            count ← control.length - shown > sizeof(viewport) ? sizeof(viewport) : (size_t)(control.length - shown);
            memcpy(viewport, control.bytes + shown, count);
        } else assert(read_response_window(&conversation, shown, streaming, viewport, sizeof(viewport), &count) == FC_OK);
        assert(count);
        double now ← clock_seconds(CLOCK_MONOTONIC);
        if (first_visible < 0) first_visible ← now - began;
        if (first_final < 0) first_final ← now - terminal;
        displayed_crc ← checksum_bytes(displayed_crc, viewport, count);
        shown ← shown + count;
    }
    double drain_latency ← clock_seconds(CLOCK_MONOTONIC) - terminal;
    struct rusage usage;
    assert(getrusage(RUSAGE_SELF, &usage) == 0);
    if (ram) {
        printf("mode\tpeak_rss_kib\tresponse_bytes\tdata_bytes\tjournal_bytes\twrites\tmax_write\tmax_view\tfirst_text_ms\tterminal_first_window_ms\tterminal_drain_ms\tgeneration_cpu_ms\treplay_ms\tresponse_crc\n");
        printf("ram\t%ld\t%zu\t0\t0\t0\t0\t%u\t%.3f\t%.3f\t%.3f\t%.3f\tNOT_APPLICABLE\t%08x\n",
               usage.ru_maxrss, control.length, FC_VIEW_BYTES, first_visible * 1000,
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
        printf("mode\tpeak_rss_kib\tresponse_bytes\tdata_bytes\tjournal_bytes\twrites\tmax_write\tmax_view\tfirst_text_ms\tterminal_first_window_ms\tterminal_drain_ms\tgeneration_cpu_ms\treplay_ms\tresponse_crc\n");
        printf("%s\t%ld\t%llu\t%llu\t%llu\t%llu\t%zu\t%zu\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%08x\n",
               mode, usage.ru_maxrss, (unsigned long long)shown,
               (unsigned long long)measurements.data_bytes, (unsigned long long)measurements.journal_bytes,
               (unsigned long long)measurements.write_count, measurements.maximum_write, measurements.maximum_view,
               first_visible * 1000, first_final * 1000, drain_latency * 1000,
               generating_cpu * 1000, replay_time * 1000, displayed_crc);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "benchmark") == 0)
        return benchmark_response(argv[2], argv[3]);

    response_cases();
    cancellation_cases();
    interrupted_cases();
    process_death_case();
    journal_cases();
    bounded_cases();
    ram_control_case();
    puts("PASS C vertical core");
    return 0;
}
