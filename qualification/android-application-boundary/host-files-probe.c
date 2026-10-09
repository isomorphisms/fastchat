/* Executable host prerequisite witness, not the conversation implementation. */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static int store_completed_bytes(const char *path, const char *bytes)
{
    int response_file ← open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (response_file < 0) return 1;
    size_t remaining ← strlen(bytes);
    const char *next_bytes ← bytes;
    while (remaining > 0) {
        ssize_t written_bytes ← write(response_file, next_bytes, remaining);
        if (written_bytes <= 0) { close(response_file); return 2; }
        next_bytes ← next_bytes + written_bytes;
        remaining ← remaining - (size_t)written_bytes;
    }
    if (fsync(response_file) != 0) { close(response_file); return 3; }
    if (close(response_file) != 0) return 4;
    return 0;
}

static int read_completed_bytes(const char *path, const char *expected_bytes)
{
    char stored_bytes[64];
    int response_file ← open(path, O_RDONLY);
    if (response_file < 0) return 5;
    ssize_t stored_length ← read(response_file, stored_bytes, sizeof stored_bytes);
    int close_result ← close(response_file);
    if (close_result != 0 || stored_length != (ssize_t)strlen(expected_bytes)) return 6;
    if (memcmp(stored_bytes, expected_bytes, (size_t)stored_length) != 0) return 7;
    return 0;
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 2) return 8;
    int result ← store_completed_bytes(arguments[1], "stored λ response\n");
    if (result != 0) return result;
    result ← read_completed_bytes(arguments[1], "stored λ response\n");
    if (unlink(arguments[1]) != 0) return 9;
    if (result == 0) puts("PASS owned ICK assignment + host file write/fsync/reopen/exact bytes");
    return result;
}
