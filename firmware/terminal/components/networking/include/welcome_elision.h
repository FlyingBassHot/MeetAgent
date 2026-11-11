/*
 * Welcome/events elision (pure logic, no cJSON, no FreeRTOS).
 *
 * The Node welcome can be tens of KB because snapshot.events replays recent
 * events. The terminal only needs node/agents/sessions, so the events array is
 * replaced with [] in the raw JSON before cJSON sees it.
 *
 * The result distinguishes "no events key" from "events: []" so callers do not
 * reject a large welcome that legitimately has no replay events.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool found;     /* an "events" key was found */
    bool malformed; /* key found but value is not a closed array */
    int count;      /* top-level items in the array before elision */
} meet_json_elision_t;

/* Finds a string key at any depth; returns the value start (after ':'). */
const char *meet_json_find_key(const char *json, const char *key);

/*
 * Replaces the first "events" array body with an empty array in place.
 * The buffer must be NUL-terminated and writable.
 */
meet_json_elision_t meet_json_elide_events(char *json);

/*
 * True when the welcome must be rejected: malformed events, or a large
 * welcome with no events key at all. A large welcome with events: [] passes.
 */
bool meet_welcome_elision_reject(size_t bytes,
                                 const meet_json_elision_t *result);

#ifdef __cplusplus
}
#endif
