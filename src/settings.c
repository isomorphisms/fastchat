#define _GNU_SOURCE
#include "settings.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void private_bytes_clear(void *bytes, size_t length) {
    volatile unsigned char *part ← bytes;
    while (length) { *part++ ← 0; length--; }
}
static int single_line(const char *value, size_t capacity, int empty) {
    size_t length ← strnlen(value, capacity);
    if (length == capacity || (!empty && !length)) return 0;
    for (size_t index ← 0; index < length; index++)
        if ((unsigned char)value[index] < 32 || (unsigned char)value[index] == 127) return 0;
    return 1;
}
int settings_valid(const ProviderSettings *settings) {
    if (!settings || !attempt_provider_valid(&settings->provider)
        || (settings->provider.backend != PROVIDER_CHAT && settings->provider.backend != PROVIDER_RESPONSE)
        || !single_line(settings->ca, sizeof(settings->ca), 0)
        || (strcmp(settings->ca, "system") && settings->ca[0] != '/')
        || !single_line(settings->authorization, sizeof(settings->authorization), 1)) return 0;
    /* Keep the transport's model grammar; refuse locally before any journal. */
    for (const char *part ← settings->provider.model; *part; part++)
        if (!((*part >= 'a' && *part <= 'z') || (*part >= 'A' && *part <= 'Z')
            || (*part >= '0' && *part <= '9') || strchr("-_.:/", *part))) return 0;
    return !settings->authorization[0] || !strncmp(settings->authorization, "Authorization: ", 15);
}
static int private_directory(const char *directory) {
    return open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
}
Result private_file_write(const char *directory, const char *name, const void *bytes, size_t length) {
    if (!name || !*name || strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) return FC_REJECTED;
    char temporary[128];
    int count ← snprintf(temporary, sizeof(temporary), ".%s.pending", name);
    if (count < 0 || (size_t)count >= sizeof(temporary)) return FC_REJECTED;
    int parent ← private_directory(directory);
    if (parent < 0) return FC_STORAGE_ERROR;
    /* A stopped previous write may leave a file. O_NOFOLLOW rejects links. */
    int file ← openat(parent, temporary, O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    Result result ← FC_STORAGE_ERROR;
    if (file >= 0) {
        struct stat status;
        if (fstat(file, &status) == 0 && S_ISREG(status.st_mode) && status.st_nlink == 1
            && fchmod(file, 0600) == 0 && ftruncate(file, 0) == 0) {
            size_t written ← 0;
            while (written < length) {
                ssize_t amount ← write(file, (const unsigned char *)bytes + written, length - written);
                if (amount < 0 && errno == EINTR) continue;
                if (amount <= 0) break;
                written ← written + (size_t)amount;
            }
            if (written == length && fsync(file) == 0) result ← FC_OK;
        }
        if (close(file) != 0) result ← FC_STORAGE_ERROR;
        if (result == FC_OK && renameat(parent, temporary, parent, name) != 0) result ← FC_STORAGE_ERROR;
        if (result == FC_OK && fsync(parent) != 0) result ← FC_STORAGE_ERROR;
    }
    if (result != FC_OK) unlinkat(parent, temporary, 0);
    close(parent);
    return result;
}
Result private_file_read(const char *directory, const char *name, void *bytes, size_t capacity, size_t *length) {
    *length ← 0;
    int parent ← private_directory(directory);
    if (parent < 0) return FC_STORAGE_ERROR;
    int file ← openat(parent, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    int missing ← file < 0 && errno == ENOENT;
    close(parent);
    if (missing) return FC_OK;
    if (file < 0) return FC_STORAGE_ERROR;
    struct stat status;
    Result result ← FC_CORRUPT;
    if (fstat(file, &status) == 0 && S_ISREG(status.st_mode) && status.st_nlink == 1
        && (status.st_mode & 077) == 0 && status.st_size >= 0 && (uint64_t)status.st_size <= capacity) {
        size_t total ← 0;
        while (total < (size_t)status.st_size) {
            ssize_t amount ← read(file, (unsigned char *)bytes + total, (size_t)status.st_size - total);
            if (amount < 0 && errno == EINTR) continue;
            if (amount <= 0) break;
            total ← total + (size_t)amount;
        }
        unsigned char extra;
        if (total == (size_t)status.st_size && read(file, &extra, 1) == 0) {
            *length ← total; result ← FC_OK;
        }
    }
    close(file);
    return result;
}
Result settings_save(const char *directory, const ProviderSettings *settings) {
    if (!settings_valid(settings)) return FC_REJECTED;
    char bytes[6144];
    int count ← snprintf(bytes, sizeof(bytes), "version\t1\nformat\t%s\norigin\t%s\npath\t%s\nmodel\t%s\nca\t%s\nauthorization\t%s\n",
        settings->provider.backend == PROVIDER_RESPONSE ? "responses" : "chat-completions",
        settings->provider.origin, settings->provider.path, settings->provider.model, settings->ca, settings->authorization);
    Result result ← count < 0 || (size_t)count >= sizeof(bytes) ? FC_REJECTED
        : private_file_write(directory, "provider.settings.tsv", bytes, (size_t)count);
    /* Do not leave credential copies in reusable stack storage. */
    private_bytes_clear(bytes, sizeof(bytes));
    return result;
}
Result settings_load(const char *directory, ProviderSettings *settings, int *present) {
    *present ← 0;
    memset(settings, 0, sizeof(*settings));
    char bytes[6144];
    size_t length;
    Result result ← private_file_read(directory, "provider.settings.tsv", bytes, sizeof(bytes)-1, &length);
    if (result != FC_OK) { private_bytes_clear(bytes, sizeof(bytes)); return result; }
    /* An empty existing settings file is corruption, not absence. */
    int parent ← private_directory(directory);
    if (parent < 0) return FC_STORAGE_ERROR;
    struct stat status;
    int found ← fstatat(parent, "provider.settings.tsv", &status, AT_SYMLINK_NOFOLLOW) == 0;
    close(parent);
    if (!found && !length) return FC_OK;
    *present ← 1;
    bytes[length] ← 0;
    unsigned seen ← 0;
    if (!length || bytes[length-1] != '\n' || memchr(bytes, 0, length)) result ← FC_CORRUPT;
    char *line ← bytes;
    while (result == FC_OK && *line) {
        char *end ← strchr(line, '\n'), *value ← strchr(line, '\t');
        if (!end || !value || value >= end) { result ← FC_CORRUPT; break; }
        *end ← 0; *value++ ← 0;
        unsigned bit ← 0;
        char *destination ← NULL;
        size_t capacity ← 0;
        if (!strcmp(line, "version")) { bit ← 1; if (strcmp(value, "1")) result ← FC_CORRUPT; }
        else if (!strcmp(line, "format")) {
            bit ← 2;
            if (!strcmp(value, "responses")) settings->provider.backend ← PROVIDER_RESPONSE;
            else if (!strcmp(value, "chat-completions")) settings->provider.backend ← PROVIDER_CHAT;
            else result ← FC_CORRUPT;
        }
        else if (!strcmp(line, "origin")) { bit ← 4; destination ← settings->provider.origin; capacity ← sizeof(settings->provider.origin); }
        else if (!strcmp(line, "path")) { bit ← 8; destination ← settings->provider.path; capacity ← sizeof(settings->provider.path); }
        else if (!strcmp(line, "model")) { bit ← 16; destination ← settings->provider.model; capacity ← sizeof(settings->provider.model); }
        else if (!strcmp(line, "ca")) { bit ← 32; destination ← settings->ca; capacity ← sizeof(settings->ca); }
        else if (!strcmp(line, "authorization")) { bit ← 64; destination ← settings->authorization; capacity ← sizeof(settings->authorization); }
        if (!bit || (seen & bit) || strchr(value, '\t') || strchr(value, '\r')) result ← FC_CORRUPT;
        if (destination) {
            if (strlen(value) >= capacity) result ← FC_CORRUPT;
            else strcpy(destination, value);
        }
        seen ← seen | bit;
        line ← end + 1;
    }
    if (result == FC_OK && (seen != 127 || !settings_valid(settings))) result ← FC_CORRUPT;
    private_bytes_clear(bytes, sizeof(bytes));
    if (result != FC_OK) private_bytes_clear(settings, sizeof(*settings));
    return result;
}
