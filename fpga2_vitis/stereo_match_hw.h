#ifndef STEREO_MATCH_HW_H
#define STEREO_MATCH_HW_H

#include "xil_io.h"
#include "xil_types.h"

#define STM_ID              0x00U
#define STM_STATUS          0x04U
#define STM_DEST0           0x08U
#define STM_DEST1           0x0CU
#define STM_DEST2           0x10U
#define STM_FRAME_SEL       0x14U
#define STM_WIDTH           0x18U
#define STM_HEIGHT          0x1CU
#define STM_STRIDE          0x20U
#define STM_UNIQUENESS      0x24U
#define STM_PIXEL_COUNT     0x28U
#define STM_CYCLE_COUNT     0x2CU
#define STM_ERROR_CODE      0x30U

#define STM_CORE_ID         0x53544D31U
#define STM_STATUS_BUSY     0x1U
#define STM_STATUS_DONE     0x2U
#define STM_STATUS_ERROR    0x4U

static inline void StmWrite(u32 base, u32 offset, u32 value)
{
	Xil_Out32(base + offset, value);
}

static inline u32 StmRead(u32 base, u32 offset)
{
	return Xil_In32(base + offset);
}

#endif
