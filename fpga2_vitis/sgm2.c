#include "sgm2.h"
static int valid(const sgm2_device *d) { return d && d->read32 && d->write32; }
int sgm2_start(const sgm2_device *d, uint32_t destination, uint32_t stride,
               uint16_t width, uint16_t height) {
    uint32_t status;
    uint64_t end;
    if (!valid(d) || !width || (width & 3) || !height ||
        (destination & 511) || (stride & 511) || stride < (uint32_t)width*2)
        return SGM2_E_ARGUMENT;
    end=(uint64_t)destination+(uint64_t)(height-1)*stride+(uint32_t)width*2;
    if (end>UINT64_C(0x100000000)) return SGM2_E_ARGUMENT;
    if (d->read32(d->context,SGM2_ID)!=UINT32_C(0x53474d32)) return SGM2_E_VERSION;
    if (d->read32(d->context,SGM2_SIZE)!=((uint32_t)height<<16 | width)) return SGM2_E_SIZE;
    status=d->read32(d->context,SGM2_STATUS);
    if (status & (SGM2_FATAL|SGM2_BUS_FAULT)) return SGM2_E_FATAL;
    if (!(status & SGM2_READY) || (status & SGM2_BUSY)) return SGM2_E_NOT_READY;
    d->write32(d->context,SGM2_BASE,destination);
    d->write32(d->context,SGM2_STRIDE,stride);
    d->write32(d->context,SGM2_COMMAND,1);
    return SGM2_OK;
}
int sgm2_poll(const sgm2_device *d, sgm2_result *r) {
    uint32_t size,expected_pixels;
    if (!valid(d) || !r) return SGM2_E_ARGUMENT;
    r->status=d->read32(d->context,SGM2_STATUS);
    r->errors=0;r->reference_errors=0;r->search_errors=0;r->pixels=0;
    if (r->status & (SGM2_FATAL|SGM2_BUS_FAULT)) return SGM2_E_FATAL;
    if ((r->status & SGM2_BUSY) || !(r->status & SGM2_DONE)) return SGM2_PENDING;
    r->errors=d->read32(d->context,SGM2_ERRORS);
    r->reference_errors=d->read32(d->context,SGM2_REFERENCE_ERRORS);
    r->search_errors=d->read32(d->context,SGM2_SEARCH_ERRORS);
    r->pixels=d->read32(d->context,SGM2_PIXELS);
    size=d->read32(d->context,SGM2_SIZE);
    expected_pixels=(size>>16)*(size&65535u);
    return (r->errors || r->reference_errors || r->search_errors ||
            !expected_pixels || r->pixels!=expected_pixels)?SGM2_E_FRAME:SGM2_OK;
}
