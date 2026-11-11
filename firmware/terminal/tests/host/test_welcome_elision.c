#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_assert.h"
#include "welcome_elision.h"

static char *make_welcome(size_t filler, const char *events)
{
    const char *head =
        "{\"type\":\"welcome\",\"payload\":{\"snapshot\":{\"node\":"
        "{\"node_id\":\"n\"},\"agents\":[],";
    const char *filler_key = "\"filler\":\"";
    const char *tail = "\",\"last_sequence\":7}}}";
    size_t events_len = events != NULL ? strlen(events) : 0;
    size_t size = strlen(head) + events_len + strlen(filler_key) + filler +
                  strlen(tail) + 1;
    char *json = malloc(size);
    if (json == NULL) {
        return NULL;
    }
    size_t offset = 0;
    memcpy(json + offset, head, strlen(head));
    offset += strlen(head);
    if (events != NULL) {
        memcpy(json + offset, events, events_len);
        offset += events_len;
    }
    memcpy(json + offset, filler_key, strlen(filler_key));
    offset += strlen(filler_key);
    memset(json + offset, 'A', filler);
    offset += filler;
    memcpy(json + offset, tail, strlen(tail) + 1);
    return json;
}

static void test_missing_events_rejected_only_when_large(void)
{
    char *small = make_welcome(100, NULL);
    TEST_ASSERT_NOT_NULL(small);
    meet_json_elision_t result = meet_json_elide_events(small);
    TEST_ASSERT_FALSE(result.found);
    TEST_ASSERT_FALSE(result.malformed);
    TEST_ASSERT_EQ_INT(0, result.count);
    TEST_ASSERT_FALSE(meet_welcome_elision_reject(strlen(small), &result));
    free(small);

    char *large = make_welcome(25000, NULL);
    TEST_ASSERT_NOT_NULL(large);
    result = meet_json_elide_events(large);
    TEST_ASSERT_FALSE(result.found);
    TEST_ASSERT_TRUE(strlen(large) > 20000);
    TEST_ASSERT_TRUE(meet_welcome_elision_reject(strlen(large), &result));
    free(large);
}

static void test_empty_events_is_found_and_allowed(void)
{
    char *json = make_welcome(25000, "\"events\":[],");
    TEST_ASSERT_NOT_NULL(json);
    meet_json_elision_t result = meet_json_elide_events(json);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_FALSE(result.malformed);
    TEST_ASSERT_EQ_INT(0, result.count);
    TEST_ASSERT_FALSE(meet_welcome_elision_reject(strlen(json), &result));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"events\":[]"));
    free(json);
}

static void test_events_are_counted_and_removed(void)
{
    char *json = make_welcome(
        0, "\"events\":[{\"sequence\":1},{\"x\":[1,2]},\"text\"],");
    TEST_ASSERT_NOT_NULL(json);
    meet_json_elision_t result = meet_json_elide_events(json);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_FALSE(result.malformed);
    TEST_ASSERT_EQ_INT(3, result.count);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"events\":[]"));
    TEST_ASSERT_NULL(strstr(json, "\"sequence\":1"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"last_sequence\":7"));
    free(json);
}

static void test_malformed_events_are_rejected(void)
{
    char *unclosed = make_welcome(0, "\"events\":[{\"a\":1},");
    TEST_ASSERT_NOT_NULL(unclosed);
    meet_json_elision_t result = meet_json_elide_events(unclosed);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_TRUE(result.malformed);
    TEST_ASSERT_TRUE(meet_welcome_elision_reject(strlen(unclosed), &result));
    free(unclosed);

    char *not_array = make_welcome(0, "\"events\":null,");
    TEST_ASSERT_NOT_NULL(not_array);
    result = meet_json_elide_events(not_array);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_TRUE(result.malformed);
    TEST_ASSERT_TRUE(meet_welcome_elision_reject(strlen(not_array), &result));
    free(not_array);

    TEST_ASSERT_TRUE(meet_welcome_elision_reject(1, NULL));
}

static void test_strings_and_escapes_do_not_confuse_elision(void)
{
    char *json = make_welcome(
        0, "\"events\":[{\"text\":\"] [ \\\" events \\\"\"},{\"a\":[2]}],");
    TEST_ASSERT_NOT_NULL(json);
    meet_json_elision_t result = meet_json_elide_events(json);
    TEST_ASSERT_TRUE(result.found);
    TEST_ASSERT_FALSE(result.malformed);
    TEST_ASSERT_EQ_INT(2, result.count);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"events\":[]"));
    /* The nested key in the removed array must not be used as the target. */
    TEST_ASSERT_NULL(strstr(json, "text"));
    free(json);
}

void test_welcome_elision_run(void)
{
    test_missing_events_rejected_only_when_large();
    test_empty_events_is_found_and_allowed();
    test_events_are_counted_and_removed();
    test_malformed_events_are_rejected();
    test_strings_and_escapes_do_not_confuse_elision();
}
