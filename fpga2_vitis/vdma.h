#ifndef VDMA_H_
#define VDMA_H_

#include "xaxivdma.h"


int vdma_read_init(short DeviceID,short HoriSizeInput,short VertSizeInput,short Stride,unsigned int FrameStoreStartAddr);
int vdma_write_init(XAxiVdma *Vdma,short DeviceID,short HoriSizeInput,short VertSizeInput,short Stride,unsigned int FrameStoreStartAddr);

/*
 * One-shot export helper.  This is intentionally separate from
 * vdma_write_init(), which remains the normal continuous S2MM function.
 */
int vdma_write_one_frame_init(XAxiVdma *Vdma, short DeviceID,
		short HoriSizeInput, short VertSizeInput, short Stride,
		unsigned int FrameStoreStartAddr);
u32 vdma_version();

#endif /* VDMA_H_ */
