#ifndef SGM2_H
#define SGM2_H
#include <stdint.h>
/* MMIO callbacks are supplied by the BSP integration; offsets are byte offsets.
 * Caller owns input scheduling, cache maintenance, buffer reservation, and timeout.
 * Not thread safe: one owner per device. No automatic restart or local reset. */
typedef struct {
    void *context;
    uint32_t (*read32)(void *context, uint32_t offset);
    void (*write32)(void *context, uint32_t offset, uint32_t value);
} sgm2_device;
enum {
    SGM2_ID=0x00, SGM2_BUILD=0x04, SGM2_COMMAND=0x08, SGM2_STATUS=0x0c,
    SGM2_ERRORS=0x10, SGM2_BASE=0x14, SGM2_STRIDE=0x18, SGM2_SIZE=0x1c,
    SGM2_REFERENCE_ERRORS=0x20, SGM2_SEARCH_ERRORS=0x24, SGM2_PIXELS=0x28
};
enum { SGM2_BUSY=1, SGM2_DONE=2, SGM2_FATAL=4, SGM2_READY=8, SGM2_BUS_FAULT=16 };
enum { SGM2_OK=0, SGM2_PENDING=1, SGM2_E_ARGUMENT=-1, SGM2_E_VERSION=-2,
       SGM2_E_SIZE=-3, SGM2_E_NOT_READY=-4, SGM2_E_FATAL=-5, SGM2_E_FRAME=-6 };
typedef struct {
    uint32_t status, errors, reference_errors, search_errors, pixels;
} sgm2_result;
int sgm2_start(const sgm2_device *dev, uint32_t destination, uint32_t stride,
               uint16_t expected_width, uint16_t expected_height);
/* Call only for a successfully submitted frame; DONE is sticky until next start.
 * A pending return requires the caller's timer; expiry must NOT reset AXI locally. */
int sgm2_poll(const sgm2_device *dev, sgm2_result *result);
#endif
