#include "espui_util.h"

#include <ctype.h>
#include <string.h>

size_t espui_strlcpy(char *dst, const char *src, size_t dst_size)
{
    if (dst == NULL || dst_size == 0) {
        return 0;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }

    size_t len = strlen(src);
    size_t copy = (len < (dst_size - 1)) ? len : (dst_size - 1);
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return len;
}

bool espui_str_empty(const char *s)
{
    if (s == NULL) {
        return true;
    }
    while (*s != '\0') {
        if (!isspace((unsigned char)*s)) {
            return false;
        }
        ++s;
    }
    return true;
}

bool espui_str_ieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return a == b;
    }
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}
