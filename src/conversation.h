#ifndef FASTCHAT_CONVERSATION_H
#define FASTCHAT_CONVERSATION_H
#include <stddef.h>
#include <stdint.h>
enum { FC_PROMPT_BYTES ← 1024, FC_WRITE_BYTES ← 4096, FC_BATCH_BYTES ← 16384, FC_VIEW_BYTES ← 4096 };
typedef enum { FC_OK, FC_REJECTED, FC_BACKPRESSURE, FC_STORAGE_ERROR, FC_CORRUPT, FC_INVALID_TEXT, FC_POLICY_ERROR } Result;
typedef enum { IDLE, SUBMITTED, GENERATING, CANCEL_PENDING, UNCERTAIN, COMPLETED, CANCELLED, FAILED } Phase;
typedef enum { SUBMIT ← 1, START, PREFIX, COMPLETE, CANCEL_REQUEST, CANCEL_ACK, FAILURE, TRANSPORT_LOSS, RETRY, ATTEMPT_SUBMIT, TOOL } Event;
typedef struct { uint64_t stream; uint64_t offset; } AppendAddress;
typedef struct { Result result; size_t consumed; } WriteResult;
typedef struct { uint64_t capacity, written, durable, committed, received, eligible; } Extents;
typedef struct {
    uint64_t data_bytes, journal_bytes, write_count, barrier_count;
    uint64_t reserve_bytes, reserve_calls, reserve_writes;
    uint64_t allocation_bytes, allocation_calls;
    size_t maximum_write, maximum_view;
} StoreMeasurements;
typedef struct {
    int directory, journal, response;
    uint64_t conversation, request, attempt, sequence, journal_end;
    uint64_t response_base, arena_high_water, arena_capacity;
    uint32_t response_crc;
    uint32_t utf8_code, utf8_min;
    unsigned utf8_needed;
    Phase phase;
    Extents extents;
    StoreMeasurements measurements;
    char prompt[FC_PROMPT_BYTES + 1];
    size_t prompt_length;
    int poisoned, recovered_partial, response_started, pending_submission;
    /* Deterministic fault controls; -1 disables the byte budget. */
    int64_t write_budget;
    size_t short_write;
    int fail_barrier, would_block, fail_barrier_after;
    int crash_at; /* Hosted fault build only: 1 write, 2 response sync, 3 journal write, 4 journal sync. */
    int force_reservation_writes;
} Conversation;
Result conversation_open(Conversation *conversation, const char *directory);
void conversation_close(Conversation *conversation);
Result submit_text(Conversation *conversation, const char *text, size_t length);
Result retry_request(Conversation *conversation);
Result store_tool_event(Conversation *, uint64_t request, uint64_t attempt, const char *, size_t);
const char *event_name(Event);
typedef struct {
    uint64_t request, attempt, base, extent;
    Phase phase;
    char prompt[FC_PROMPT_BYTES + 1];
    size_t prompt_length;
} HistoryTurn;
Result history_turn(Conversation *, uint64_t request, HistoryTurn *);
Result read_history_window(Conversation *, const HistoryTurn *, uint64_t offset,
                           unsigned char *, size_t capacity, size_t *length);
Result admit_event(Conversation *conversation, uint64_t request, uint64_t attempt, Event event);
WriteResult store_response_bytes(Conversation *conversation, uint64_t request,
                                 uint64_t attempt, const void *bytes, size_t length);
Result commit_stored_prefix(Conversation *conversation);
int extents_are_ordered(const Conversation *conversation);
AppendAddress response_append_address(const Conversation *conversation);
Result read_response_window(Conversation *conversation, uint64_t offset, int streaming,
                            unsigned char *bytes, size_t capacity, size_t *length);
const char *phase_name(Phase phase);
uint32_t checksum_bytes(uint32_t checksum, const void *bytes, size_t length);
size_t complete_utf8_prefix(const unsigned char *bytes, size_t length, int *valid);
#endif
