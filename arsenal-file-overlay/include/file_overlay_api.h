/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 noire342
 * Standalone utility ABI: no OpenRAC table is needed for installation.
 * Install/remove run on the owner/load thread; file reads may use any thread.
 */
#ifndef OPENRAC_FILE_OVERLAY_API_H
#define OPENRAC_FILE_OVERLAY_API_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
#ifdef __cplusplus
extern "C" {
#endif

enum file_overlay_result {
    FILE_OVERLAY_OK = 0,
    FILE_OVERLAY_ARGUMENT = -1,
    FILE_OVERLAY_BUILD = -2,
    FILE_OVERLAY_CONFIG = -3,
    FILE_OVERLAY_BACKEND = -4,
    FILE_OVERLAY_ALREADY_INSTALLED = -5,
    FILE_OVERLAY_BUSY = -6,
    FILE_OVERLAY_OS = -7
};
typedef struct file_overlay_stats_v1 {
    uint32_t size;
    uint32_t version;
    uint32_t aliases;
    uint32_t installed;
    uint64_t redirected_reads;
    uint64_t passthrough_opens;
} file_overlay_stats_v1;

/* Absolute UTF-16 path to format-1 file-overlay.json containing exactly19
 * {"original":<absolute path>,"replacement":<absolute staged path>} aliases.
 * Fails before activation on unsupported EXE/config/backend errors. */
typedef int (*openrac_file_overlay_install_fn)(const wchar_t* config_file);
typedef int (*openrac_file_overlay_remove_fn)(void);
/* Thread-local description; valid until this thread's next last_error call. */
typedef const char* (*openrac_file_overlay_last_error_fn)(void);
typedef int (*openrac_file_overlay_stats_fn)(file_overlay_stats_v1* out);

#ifdef __cplusplus
}
#endif
#endif
