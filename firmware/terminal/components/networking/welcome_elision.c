#include "welcome_elision.h"

#include <ctype.h>
#include <string.h>

const char *meet_json_find_key(const char *json, const char *key)
{
    if (json == NULL || key == NULL) {
        return NULL;
    }
    size_t key_len = strlen(key);
    const char *p = json;
    while (*p != '\0') {
        if (*p != '"') {
            ++p;
            continue;
        }
        const char *start = ++p;
        while (*p != '\0' && *p != '"') {
            if (*p == '\\' && p[1] != '\0') {
                p += 2;
            } else {
                ++p;
            }
        }
        if (*p != '"') {
            break;
        }
        size_t length = (size_t)(p - start);
        const char *after = p + 1;
        if (length == key_len && memcmp(start, key, key_len) == 0) {
            const char *q = after;
            while (isspace((unsigned char)*q)) {
                ++q;
            }
            if (*q == ':') {
                ++q;
                while (isspace((unsigned char)*q)) {
                    ++q;
                }
                return q;
            }
        }
        ++p;
    }
    return NULL;
}

static char *matching_array(char *open)
{
    if (open == NULL || *open != '[') {
        return NULL;
    }
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (char *p = open; *p != '\0'; ++p) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static int count_array_items(const char *open, const char *close)
{
    int count = 0;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    bool in_item = false;
    for (const char *p = open + 1; p < close; ++p) {
        char c = *p;
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            if (depth == 0 && !in_item) {
                ++count;
                in_item = true;
            }
            in_string = true;
        } else if (c == '[' || c == '{') {
            if (depth == 0 && !in_item) {
                ++count;
                in_item = true;
            }
            ++depth;
        } else if (c == ']' || c == '}') {
            if (depth > 0) {
                --depth;
            }
        } else if (depth == 0) {
            if (c == ',') {
                in_item = false;
            } else if (!isspace((unsigned char)c) && !in_item) {
                ++count;
                in_item = true;
            }
        }
    }
    return count;
}

meet_json_elision_t meet_json_elide_events(char *json)
{
    meet_json_elision_t result = {0};
    char *value = (char *)meet_json_find_key(json, "events");
    if (value == NULL) {
        return result;
    }
    result.found = true;
    if (*value != '[') {
        result.malformed = true;
        return result;
    }
    char *close = matching_array(value);
    if (close == NULL) {
        result.malformed = true;
        return result;
    }
    result.count = count_array_items(value, close);
    memmove(value + 1, close, strlen(close) + 1);
    return result;
}

bool meet_welcome_elision_reject(size_t bytes,
                                 const meet_json_elision_t *result)
{
    if (result == NULL || result->malformed) {
        return true;
    }
    return !result->found && bytes > 20000;
}
