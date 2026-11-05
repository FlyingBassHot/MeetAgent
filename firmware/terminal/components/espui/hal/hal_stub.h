#pragma once

#include "espui_display.h"
#include "espui_input.h"
#include "espui_power.h"
#include "espui_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

const espui_display_hal_t *espui_hal_display_stub(void);
const espui_touch_hal_t *espui_hal_touch_stub(void);
const espui_input_hal_t *espui_hal_input_stub(void);
const espui_power_hal_t *espui_hal_power_stub(void);

#ifdef __cplusplus
}
#endif
