#ifndef FASTCHAT_SETTINGS_H
#define FASTCHAT_SETTINGS_H
#include "conversation.h"

typedef struct {
    AttemptProvider provider;
    char ca[2048];
    char authorization[2048];
} ProviderSettings;

int settings_valid(const ProviderSettings *);
void private_bytes_clear(void *, size_t);
/* Missing settings are reported separately; malformed files never select demo. */
Result settings_load(const char *directory, ProviderSettings *, int *present);
Result settings_save(const char *directory, const ProviderSettings *);
Result private_file_write(const char *directory, const char *name, const void *, size_t);
Result private_file_read(const char *directory, const char *name, void *, size_t, size_t *);
#endif
