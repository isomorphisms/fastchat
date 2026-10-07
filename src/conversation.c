#define _GNU_SOURCE
#include "conversation.h"
#include "appendfat_arena.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
enum { RECORD_HEADER ← 64, RECORD_BYTES ← RECORD_HEADER + FC_PROMPT_BYTES, RESERVATION_BYTES ← 65536 };

uint32_t checksum_bytes(uint32_t checksum, const void *bytes, size_t length) {
    const unsigned char *input ← bytes;
    for (size_t index ← 0; index < length; index ← index + 1) {
        checksum ← checksum ^ input[index];
        for (unsigned bit ← 0; bit < 8; bit ← bit + 1)
            checksum ← (checksum >> 1) ^ (0xedb88320u & (0u - (checksum & 1u)));
    }
    return checksum;
}
static void encode_number(unsigned char *destination, uint64_t value, size_t length) {
    for (size_t index ← 0; index < length; index ← index + 1) {
        destination[index] ← (unsigned char)value;
        value ← value >> 8;
    }
}
static uint64_t decode_number(const unsigned char *source, size_t length) {
    uint64_t value ← 0;
    for (size_t index ← length; index > 0; index ← index - 1)
        value ← (value << 8) | source[index - 1];
    return value;
}
static int read_exact(int file, void *bytes, size_t length, uint64_t offset) {
    unsigned char *destination ← bytes;
    size_t read_bytes ← 0;
    while (read_bytes < length) {
        ssize_t count ← pread(file, destination + read_bytes, length - read_bytes,
                             (off_t)(offset + read_bytes));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return 0;
        read_bytes ← read_bytes + (size_t)count;
    }
    return 1;
}
static Result storage_failure(Conversation *conversation) {
    conversation->poisoned ← 1;
    return FC_STORAGE_ERROR;
}
static Result durability_barrier(Conversation *conversation, int file) {
    if (conversation->fail_barrier) return storage_failure(conversation);
    int result;
    do { result ← fsync(file); } while (result < 0 && errno == EINTR);
    if (result < 0) return storage_failure(conversation);
    conversation->measurements.barrier_count ← conversation->measurements.barrier_count + 1;
    return FC_OK;
}
static WriteResult write_at(Conversation *conversation, int file, const void *bytes,
                            size_t length, uint64_t offset, int journal) {
    const unsigned char *source ← bytes;
    WriteResult result ← { FC_OK, 0 };
    while (result.consumed < length) {
        if (conversation->would_block) { result.result ← FC_BACKPRESSURE; return result; }
        size_t amount ← length - result.consumed;
        if (amount > FC_WRITE_BYTES) amount ← FC_WRITE_BYTES;
        if (conversation->short_write && amount > conversation->short_write)
            amount ← conversation->short_write;
        if (conversation->write_budget == 0) {
            errno ← ENOSPC;
            result.result ← storage_failure(conversation);
            return result;
        }
        if (conversation->write_budget > 0 && amount > (uint64_t)conversation->write_budget)
            amount ← (size_t)conversation->write_budget;
        ssize_t count ← pwrite(file, source + result.consumed, amount,
                              (off_t)(offset + result.consumed));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            result.result ← FC_BACKPRESSURE;
            return result;
        }
        if (count <= 0) { result.result ← storage_failure(conversation); return result; }
        result.consumed ← result.consumed + (size_t)count;
        if (conversation->write_budget > 0) conversation->write_budget ← conversation->write_budget - count;
        conversation->measurements.write_count ← conversation->measurements.write_count + 1;
        if ((size_t)count > conversation->measurements.maximum_write)
            conversation->measurements.maximum_write ← (size_t)count;
        if (journal) conversation->measurements.journal_bytes ← conversation->measurements.journal_bytes + count;
        else conversation->measurements.data_bytes ← conversation->measurements.data_bytes + count;
    }
    return result;
}
int extents_are_ordered(const Conversation *conversation) {
    const Extents *extent ← &conversation->extents;
    return extent->committed <= extent->durable && extent->durable <= extent->written
        && extent->written <= extent->capacity && extent->eligible <= extent->committed;
}
AppendAddress response_append_address(const Conversation *conversation) {
    return (AppendAddress){ conversation->attempt, conversation->extents.written };
}
static Result prepare_attempt(Conversation *conversation, uint64_t attempt) {
    (void)attempt;
    if (conversation->arena_high_water > conversation->arena_capacity)
        return FC_CORRUPT;
    conversation->response_base ← conversation->arena_high_water;
    conversation->extents.capacity ←
        conversation->arena_capacity - conversation->response_base;
    return FC_OK;
}
static int terminal_phase(Phase phase) {
    return phase == COMPLETED || phase == CANCELLED || phase == FAILED;
}
static int response_phase(const Conversation *conversation) {
    return conversation->phase == GENERATING || conversation->phase == CANCEL_PENDING;
}
static int event_is_admissible(const Conversation *conversation, Event event) {
    Phase phase ← conversation->phase;
    if (event == SUBMIT) return phase == IDLE || terminal_phase(phase);
    if (event == RETRY) return phase == UNCERTAIN;
    if (event == START) return phase == SUBMITTED || phase == CANCEL_PENDING;
    if (event == CANCEL_REQUEST) return phase == SUBMITTED || phase == GENERATING;
    if (event == TRANSPORT_LOSS || event == FAILURE) return phase == SUBMITTED || phase == CANCEL_PENDING || phase == GENERATING;
    if (event == CANCEL_ACK) return phase == GENERATING || phase == CANCEL_PENDING;
    if (event == PREFIX || event == COMPLETE) return response_phase(conversation);
    return 0;
}
static void apply_phase(Conversation *conversation, Event event) {
    if (event == SUBMIT || event == RETRY) {
        conversation->phase ← SUBMITTED;
        conversation->response_started ← 0;
    }
    if (event == START) {
        conversation->response_started ← 1;
        if (conversation->phase != CANCEL_PENDING) conversation->phase ← GENERATING;
    }
    if (event == COMPLETE) conversation->phase ← COMPLETED;
    if (event == CANCEL_REQUEST) conversation->phase ← CANCEL_PENDING;
    if (event == CANCEL_ACK) conversation->phase ← CANCELLED;
    if (event == FAILURE) conversation->phase ← FAILED;
    if (event == TRANSPORT_LOSS) conversation->phase ← UNCERTAIN;
}
static Result publish_record(Conversation *conversation, Event event, uint64_t request,
                             uint64_t attempt, const char *prompt, size_t prompt_length) {
    unsigned char record[RECORD_BYTES];
    memset(record, 0, RECORD_HEADER);
    size_t length ← RECORD_HEADER + prompt_length;
    memcpy(record, "FC01", 4);
    encode_number(record + 4, length, 4);
    encode_number(record + 8, conversation->sequence + 1, 8);
    encode_number(record + 16, request, 8);
    encode_number(record + 24, attempt, 8);
    if (conversation->response_base > UINT64_MAX - conversation->extents.durable)
        return storage_failure(conversation);
    encode_number(record + 32,
                  conversation->response_base + conversation->extents.durable, 8);
    encode_number(record + 40, conversation->response_crc, 4);
    encode_number(record + 44, event, 4);
    encode_number(record + 48, prompt_length, 4);
    encode_number(record + 52, conversation->conversation, 8);
    if (prompt_length) memcpy(record + RECORD_HEADER, prompt, prompt_length);
    encode_number(record + 60, checksum_bytes(0xffffffffu, record, length), 4);
    WriteResult written ← write_at(conversation, conversation->journal, record, length, conversation->journal_end, 1);
    /* A journal fragment is never resumable by a different event. */
    if (written.result != FC_OK) return storage_failure(conversation);
    if (durability_barrier(conversation, conversation->journal) != FC_OK) return FC_STORAGE_ERROR;
    conversation->journal_end ← conversation->journal_end + length;
    conversation->sequence ← conversation->sequence + 1;
    conversation->request ← request;
    conversation->attempt ← attempt;
    conversation->extents.committed ← conversation->extents.durable;
    conversation->extents.eligible ← conversation->extents.committed;
    apply_phase(conversation, event);
    if (event == SUBMIT) {
        memcpy(conversation->prompt, prompt, prompt_length);
        conversation->prompt[prompt_length] ← 0;
        conversation->prompt_length ← prompt_length;
    }
    return FC_OK;
}
static Result validate_prefix(Conversation *conversation,
                              uint64_t absolute_extent,
                              uint32_t expected) {
    if (absolute_extent < conversation->response_base
        || absolute_extent > conversation->arena_capacity)
        return FC_CORRUPT;
    uint64_t extent ← absolute_extent - conversation->response_base;
    if (extent < conversation->extents.committed) return FC_CORRUPT;
    unsigned char bytes[FC_WRITE_BYTES];
    uint64_t position ← conversation->response_base + conversation->extents.committed;
    uint32_t checksum ← conversation->response_crc;
    while (position < absolute_extent) {
        size_t amount ← absolute_extent - position > sizeof(bytes)
            ? sizeof(bytes) : (size_t)(absolute_extent - position);
        if (!read_exact(conversation->response, bytes, amount, position))
            return FC_CORRUPT;
        checksum ← checksum_bytes(checksum, bytes, amount);
        position ← position + amount;
    }
    if (checksum != expected) return FC_CORRUPT;
    conversation->response_crc ← checksum;
    /* Uncommitted receive/write progress is unknown after restart. */
    conversation->extents.received ← extent;
    conversation->extents.written ← extent;
    conversation->extents.durable ← extent;
    conversation->extents.committed ← extent;
    conversation->extents.eligible ← extent;
    conversation->extents.capacity ←
        conversation->arena_capacity - conversation->response_base;
    if (absolute_extent > conversation->arena_high_water)
        conversation->arena_high_water ← absolute_extent;
    return FC_OK;
}
static Result replay_record(Conversation *conversation, const unsigned char *record, size_t length) {
    Event event ← (Event)decode_number(record + 44, 4);
    uint64_t request ← decode_number(record + 16, 8);
    uint64_t attempt ← decode_number(record + 24, 8);
    uint64_t extent ← decode_number(record + 32, 8);
    uint32_t checksum ← (uint32_t)decode_number(record + 40, 4);
    size_t prompt_length ← (size_t)decode_number(record + 48, 4);
    if (decode_number(record + 8, 8) != conversation->sequence + 1
        || decode_number(record + 52, 8) != conversation->conversation
        || length != RECORD_HEADER + prompt_length || !event_is_admissible(conversation, event)) return FC_CORRUPT;
    if (event == SUBMIT || event == RETRY) {
        if (attempt != conversation->attempt + 1
            || checksum != 0xffffffffu
            || extent != conversation->arena_high_water
            || request != conversation->request + (event == SUBMIT)
            || (event == RETRY && prompt_length)) return FC_CORRUPT;
        conversation->response_base ← extent;
        conversation->extents ← (Extents){0};
        conversation->extents.capacity ←
            conversation->arena_capacity - conversation->response_base;
        conversation->response_crc ← 0xffffffffu;
        if (event == SUBMIT) {
            memcpy(conversation->prompt, record + RECORD_HEADER, prompt_length);
            conversation->prompt[prompt_length] ← 0;
            conversation->prompt_length ← prompt_length;
        }
    } else if (request != conversation->request || attempt != conversation->attempt || prompt_length) return FC_CORRUPT;
    if (validate_prefix(conversation, extent, checksum) != FC_OK) return FC_CORRUPT;
    conversation->request ← request;
    conversation->attempt ← attempt;
    conversation->sequence ← conversation->sequence + 1;
    apply_phase(conversation, event);
    return FC_OK;
}
static Result replay_journal(Conversation *conversation) {
    unsigned char record[RECORD_BYTES];
    struct stat journal_status;
    if (fstat(conversation->journal, &journal_status) < 0) return storage_failure(conversation);
    uint64_t extent ← (uint64_t)journal_status.st_size;
    while (conversation->journal_end < extent) {
        uint64_t remaining ← extent - conversation->journal_end;
        if (remaining < RECORD_HEADER) break;
        if (!read_exact(conversation->journal, record, RECORD_HEADER, conversation->journal_end)) return FC_CORRUPT;
        size_t length ← (size_t)decode_number(record + 4, 4);
        if (memcmp(record, "FC01", 4) || length < RECORD_HEADER || length > RECORD_BYTES) return FC_CORRUPT;
        if (remaining < length) break;
        if (!read_exact(conversation->journal, record, length, conversation->journal_end)) return FC_CORRUPT;
        uint32_t expected ← (uint32_t)decode_number(record + 60, 4);
        memset(record + 60, 0, 4);
        if (checksum_bytes(0xffffffffu, record, length) != expected) return FC_CORRUPT;
        if (replay_record(conversation, record, length) != FC_OK) return FC_CORRUPT;
        conversation->journal_end ← conversation->journal_end + length;
    }
    if (conversation->journal_end < extent) {
        if (ftruncate(conversation->journal, (off_t)conversation->journal_end) < 0
            || durability_barrier(conversation, conversation->journal) != FC_OK) return FC_STORAGE_ERROR;
    }
    if (conversation->response >= 0) {
        if (conversation->response_base > conversation->arena_capacity
            || conversation->arena_high_water > conversation->arena_capacity)
            return FC_CORRUPT;
        conversation->extents.capacity ←
            conversation->arena_capacity - conversation->response_base;
        if (!extents_are_ordered(conversation)) return FC_CORRUPT;
    }
    if (conversation->phase == SUBMITTED || conversation->phase == GENERATING || conversation->phase == CANCEL_PENDING) {
        conversation->recovered_partial ← 1;
        if (publish_record(conversation, TRANSPORT_LOSS, conversation->request, conversation->attempt, NULL, 0) != FC_OK)
            return FC_STORAGE_ERROR;
    }
    return FC_OK;
}
Result conversation_open(Conversation *conversation, const char *directory) {
    memset(conversation, 0, sizeof(*conversation));
    conversation->directory ← -1;
    conversation->journal ← -1;
    conversation->response ← -1;
    conversation->conversation ← 1;
    conversation->response_crc ← 0xffffffffu;
    conversation->write_budget ← -1;
    int created ← mkdir(directory, 0700);
    if (created < 0 && errno != EEXIST) return FC_STORAGE_ERROR;
    if (created == 0) {
        char parent_path[4096];
        if (strlen(directory) >= sizeof(parent_path)) return FC_STORAGE_ERROR;
        strcpy(parent_path, directory);
        char *separator ← strrchr(parent_path, '/');
        if (!separator) strcpy(parent_path, ".");
        else if (separator == parent_path) parent_path[1] ← 0;
        else *separator ← 0;
        int parent ← open(parent_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (parent < 0) return FC_STORAGE_ERROR;
        Result durable ← durability_barrier(conversation, parent);
        close(parent);
        if (durable != FC_OK) return durable;
    }
    conversation->directory ← open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (conversation->directory < 0) return FC_STORAGE_ERROR;
    conversation->journal ← openat(conversation->directory, "history.events", O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (conversation->journal < 0 || flock(conversation->journal, LOCK_EX | LOCK_NB) < 0) {
        conversation_close(conversation);
        return FC_STORAGE_ERROR;
    }
    conversation->response ← openat(conversation->directory, "responses.arena",
                                    O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (conversation->response < 0) {
        conversation_close(conversation);
        return FC_STORAGE_ERROR;
    }
    struct stat response_status;
    if (fstat(conversation->response, &response_status) < 0
        || response_status.st_size < 0) {
        conversation_close(conversation);
        return FC_STORAGE_ERROR;
    }
    conversation->arena_capacity ← (uint64_t)response_status.st_size;
    if (durability_barrier(conversation, conversation->directory) != FC_OK)
        return FC_STORAGE_ERROR;
    return replay_journal(conversation);
}
void conversation_close(Conversation *conversation) {
    if (conversation->response >= 0) close(conversation->response);
    if (conversation->journal >= 0) close(conversation->journal);
    if (conversation->directory >= 0) close(conversation->directory);
    conversation->response ← -1;
    conversation->journal ← -1;
    conversation->directory ← -1;
}
static Result begin_request(Conversation *conversation, Event event, const char *text, size_t length) {
    if (conversation->poisoned) return FC_STORAGE_ERROR;
    if (!event_is_admissible(conversation, event)) return FC_REJECTED;
    conversation->extents ← (Extents){0};
    if (prepare_attempt(conversation, conversation->attempt + 1) != FC_OK)
        return FC_STORAGE_ERROR;
    conversation->response_crc ← 0xffffffffu;
    return publish_record(conversation, event, conversation->request + (event == SUBMIT),
                          conversation->attempt + 1, text, length);
}
Result submit_text(Conversation *conversation, const char *text, size_t length) {
    if (!length || length > FC_PROMPT_BYTES) return FC_REJECTED;
    int valid;
    if (complete_utf8_prefix((const unsigned char *)text, length, &valid) != length || !valid) return FC_INVALID_TEXT;
    return begin_request(conversation, SUBMIT, text, length);
}
Result retry_request(Conversation *conversation) {
    return begin_request(conversation, RETRY, NULL, 0);
}
Result commit_stored_prefix(Conversation *conversation) {
    if (conversation->poisoned) return FC_STORAGE_ERROR;
    if (!response_phase(conversation)) return FC_REJECTED;
    if (conversation->extents.written == conversation->extents.committed) return FC_OK;
    if (durability_barrier(conversation, conversation->response) != FC_OK) return FC_STORAGE_ERROR;
    conversation->extents.durable ← conversation->extents.written;
    return publish_record(conversation, PREFIX, conversation->request, conversation->attempt, NULL, 0);
}
WriteResult store_response_bytes(Conversation *conversation, uint64_t request, uint64_t attempt,
                                 const void *bytes, size_t length) {
    WriteResult rejected ← { FC_REJECTED, 0 };
    if (conversation->poisoned) { rejected.result ← FC_STORAGE_ERROR; return rejected; }
    if (request != conversation->request || attempt != conversation->attempt || !response_phase(conversation))
        return rejected;
    if ((uint64_t)length > (uint64_t)INT64_MAX - conversation->extents.written)
        return rejected;
    uint64_t required ← conversation->extents.written + length;
    if (conversation->response_base > (uint64_t)INT64_MAX - required)
        return rejected;
    if (required > conversation->extents.received) conversation->extents.received ← required;
    WriteResult total ← { FC_OK, 0 };
    const unsigned char *source ← bytes;
    while (total.consumed < length) {
        size_t amount ← length - total.consumed;
        if (amount > FC_WRITE_BYTES) amount ← FC_WRITE_BYTES;
        size_t remaining_batch ← FC_BATCH_BYTES - (size_t)(conversation->extents.written - conversation->extents.committed);
        if (amount > remaining_batch) amount ← remaining_batch;
        uint64_t end ← conversation->extents.written + amount;
        uint64_t absolute_end ← conversation->response_base + end;
        if (end > conversation->extents.capacity) {
            uint64_t old_capacity ← conversation->arena_capacity;
            uint64_t capacity ← old_capacity;
            if (appendfat_arena_reserve_fd(conversation->response,
                    old_capacity, absolute_end, RESERVATION_BYTES, &capacity) != 0) {
                total.result ← storage_failure(conversation);
                return total;
            }
            conversation->arena_capacity ← capacity;
            conversation->extents.capacity ←
                capacity - conversation->response_base;
            conversation->measurements.reserve_bytes ←
                conversation->measurements.reserve_bytes + capacity - old_capacity;
            conversation->measurements.reserve_calls ←
                conversation->measurements.reserve_calls + 1;
        }
        AppendAddress address ← response_append_address(conversation);
        WriteResult written ← write_at(conversation, conversation->response,
                                        source + total.consumed, amount,
                                        conversation->response_base + address.offset, 0);
        conversation->response_crc ← checksum_bytes(conversation->response_crc, source + total.consumed, written.consumed);
        conversation->extents.written ← conversation->extents.written + written.consumed;
        uint64_t high_water ← conversation->response_base + conversation->extents.written;
        if (high_water > conversation->arena_high_water)
            conversation->arena_high_water ← high_water;
        total.consumed ← total.consumed + written.consumed;
        if (written.result != FC_OK) { total.result ← written.result; return total; }
        if (conversation->extents.written - conversation->extents.committed >= FC_BATCH_BYTES) {
            total.result ← commit_stored_prefix(conversation);
            if (total.result != FC_OK) return total;
        }
    }
    return total;
}
Result admit_event(Conversation *conversation, uint64_t request, uint64_t attempt, Event event) {
    if (conversation->poisoned) return FC_STORAGE_ERROR;
    if (request != conversation->request || attempt != conversation->attempt
        || !event_is_admissible(conversation, event) || event == SUBMIT || event == RETRY || event == PREFIX)
        return FC_REJECTED;
    if (event == COMPLETE && conversation->extents.received != conversation->extents.written)
        return FC_REJECTED;
    /* All metadata records reference a durable checksum, including cancel requests. */
    if (response_phase(conversation) && commit_stored_prefix(conversation) != FC_OK) return FC_STORAGE_ERROR;
    /*
     * Completion seals the attempt in the canonical journal. The shared arena
     * is deliberately not truncated or chmod-sealed: its unused reserved tail
     * is the capacity the next attempt reuses.
     */
    return publish_record(conversation, event, request, attempt, NULL, 0);
}
size_t complete_utf8_prefix(const unsigned char *bytes, size_t length, int *valid) {
    size_t position ← 0;
    *valid ← 1;
    while (position < length) {
        unsigned first ← bytes[position];
        size_t width ← first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2
            : first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (!width) { *valid ← 0; return position; }
        if (length - position < width) return position;
        unsigned codepoint ← first & (width == 1 ? 0x7f : width == 2 ? 0x1f : width == 3 ? 0x0f : 7);
        for (size_t continuation ← 1; continuation < width; continuation ← continuation + 1) {
            unsigned next ← bytes[position + continuation];
            if ((next & 0xc0) != 0x80) { *valid ← 0; return position; }
            codepoint ← (codepoint << 6) | (next & 0x3f);
        }
        if ((width == 2 && codepoint < 0x80) || (width == 3 && codepoint < 0x800)
            || (width == 4 && codepoint < 0x10000) || codepoint > 0x10ffff
            || (codepoint >= 0xd800 && codepoint <= 0xdfff)) { *valid ← 0; return position; }
        position ← position + width;
    }
    return position;
}
Result read_response_window(Conversation *conversation, uint64_t offset, int streaming,
                            unsigned char *bytes, size_t capacity, size_t *length) {
    *length ← 0;
    if (conversation->phase != COMPLETED && !streaming) return FC_OK;
    if (capacity > FC_VIEW_BYTES) capacity ← FC_VIEW_BYTES;
    uint64_t eligible ← conversation->extents.eligible;
    if (offset >= eligible || capacity == 0) return FC_OK;
    size_t amount ← eligible - offset > capacity ? capacity : (size_t)(eligible - offset);
    if (!read_exact(conversation->response, bytes, amount,
                    conversation->response_base + offset))
        return FC_STORAGE_ERROR;
    int valid;
    *length ← complete_utf8_prefix(bytes, amount, &valid);
    if (*length > conversation->measurements.maximum_view) conversation->measurements.maximum_view ← *length;
    if (!valid || (*length < amount && offset + amount == eligible && conversation->phase == COMPLETED))
        return FC_INVALID_TEXT;
    return FC_OK;
}
const char *phase_name(Phase phase) {
    static const char *names[] ← { "idle", "submitted", "generating", "cancel pending",
                                   "uncertain delivery", "completed", "cancelled", "failed" };
    return phase <= FAILED ? names[phase] : "invalid";
}
