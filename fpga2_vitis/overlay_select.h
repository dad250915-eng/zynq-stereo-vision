#ifndef OVERLAY_SELECT_H
#define OVERLAY_SELECT_H
#include <stdint.h>
/* In-place 4-connected selection. Scratch needs width*height uint32_t entries.
 * Largest component within inclusive area limits wins; ties use scan order.
 * Output is exactly 0/255. No shape/pose recognition is implied. */
uint32_t overlay_select(uint8_t *mask,unsigned width,unsigned height,
                        uint32_t min_area,uint32_t max_area,uint32_t *scratch);
#endif
