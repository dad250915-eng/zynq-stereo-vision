#pragma once
#include <stdint.h>
static const unsigned E_W=1280,E_H=720,E_N=E_W*E_H,E_POOL=626;
enum { E_AUTO=0,E_SMALL=1,E_MEDIUM=2,E_LARGE=3 };
enum { E_INVALID=1,E_RLE_OVERFLOW=2,E_META_MISMATCH=4 };
// descriptor requires 32 uint32 words; candidates requires 36 uint32 words.
// run_capacity is a count of [y,x_begin,x_end_exclusive] records, not words.
void roi_e_region_adaptive(const uint32_t *meta,const uint8_t *reference_gray,
 const uint8_t *target_mask,uint8_t *compute_mask,uint32_t *descriptor,
 uint32_t *candidates,uint32_t *runs,unsigned rows,unsigned cols,
 unsigned stride,unsigned tier_override,unsigned run_capacity,
 unsigned texture_threshold,unsigned q25_threshold);
