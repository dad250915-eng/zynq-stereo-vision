#ifndef XYZ_DIAGNOSTIC_H
#define XYZ_DIAGNOSTIC_H
#include <stdint.h>

/* Diagnostic surface sample in RIGHT-rectified camera coordinates, millimetres.
 * Not an object centre, grasp pose, base-frame position, or accuracy guarantee.
 * Inputs must be immutable, cache-coherent buffers from the same processing round.
 * Pure CPU module: no hardware, UART, motion, or large workspace allocation. */
#define XYZ_MAGIC 0x58595A31U
#define XYZ_WORDS 32U
#define XYZ_RADIUS 5U
#define XYZ_MIN_SUPPORT 800U
#define XYZ_MAX_IQR_PX 4U
enum xyz_status {
    XYZ_OK=0, XYZ_BAD_METADATA=1, XYZ_WRONG_FRAME=2, XYZ_GLOBAL_INVALID=3,
    XYZ_MASK_MISMATCH=4, XYZ_NO_INTERIOR=5, XYZ_LOW_SUPPORT=6,
    XYZ_SPREAD=7, XYZ_BAD_GEOMETRY=8, XYZ_PIPELINE_STOP=9, XYZ_BUSY=10
};
/* words: 0 magic,1 version,2 publication sequence (adapter),3 frame,4 status,
 * 5 valid,6 u,7 v,8 radius,9 patch mask pixels,10 accepted disparities,
 * 11/12/13 Q1/median/Q3 Q4,14 IQR Q4,15 support permille,
 * 16/17/18 signed X/Y/Z in 0.001 mm (NOT measurement precision),
 * 19 global depth status,20 global valid,21 selected mask pixels,
 * 22 evaluated interior patches,23 quality-passing patches,
 * 24/25 publication ticks lo/hi,26 calculation us (adapter),
 * 27 coordinate frame ID=1 (RIGHT rectified),28 units=1000 per mm,
 * 29 min support,30 max IQR Q4,31 reserved. */
void xyz_compute(const uint16_t *disparity, unsigned disparity_stride_bytes,
                 const uint8_t *mask, unsigned mask_stride_bytes,
                 const uint32_t depth[32], unsigned width, unsigned height,
                 uint32_t frame, uint32_t out[XYZ_WORDS]);
const char *xyz_status_name(uint32_t status);
#endif
