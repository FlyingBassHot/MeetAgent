/*
 * Small allocation-free helpers shared by ESPUI modules.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded string copy, always NUL-terminated when dst_size > 0. */
size_t espui_strlcpy(char *dst, const char *src, size_t dst_size);

/* True when s is NULL, empty or only whitespace. */
bool espui_str_empty(const char *s);

/* Case-insensitive equality for protocol status strings. */
bool espui_str_ieq(const char *a, const char *b);

#ifdef __cplusplus
}
#endif
