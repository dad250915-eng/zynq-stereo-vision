#ifndef FROZEN_OVERLAY_H
#define FROZEN_OVERLAY_H
#include <stdint.h>
#include <stddef.h>
enum { FO_OK=0, FO_BAD_ARGUMENT=-1, FO_BAD_RESULT=-2, FO_WRONG_FRAME=-3, FO_MASK_MISMATCH=-4 };
/* Same-frame single-target mask and depth_measure v1 result, BGRX 0x00RRGGBB.
 * Output must not alias any input. Strides are BYTES. Output has 320 leading
 * padding pixels per row for the existing centre-crop OSD full-image adapter.
 * Depth-invalid results render INVALID; malformed/stale results return error.
 */
int fo_render(const uint8_t *right, unsigned right_stride,
 const uint8_t *mask, unsigned mask_stride, const uint32_t result[32],
 unsigned width, unsigned height, uint32_t frame_id,
 uint8_t *display, unsigned display_stride);
#endif
