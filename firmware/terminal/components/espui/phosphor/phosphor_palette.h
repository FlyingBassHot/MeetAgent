#pragma once

#include <stdint.h>

#include "phosphor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t background; /* #080C0A */
    uint16_t surface;    /* #101813 */
    uint16_t primary;    /* #A7F36B */
    uint16_t secondary;  /* #62C79B */
    uint16_t muted;      /* #587364 */
    uint16_t warning;    /* #F2B75E */
    uint16_t error;      /* #F06C67 */
    uint16_t highlight;  /* #D9FFC3 */
} phosphor_palette_t;

const phosphor_palette_t *phosphor_palette_default(void);
uint16_t phosphor_rgb565_from_hex(uint32_t rgb);
uint16_t phosphor_color565(const phosphor_palette_t *palette,
                           glyph_color_id_t color);
uint16_t phosphor_scale565(uint16_t color, uint8_t intensity);

#ifdef __cplusplus
}
#endif
