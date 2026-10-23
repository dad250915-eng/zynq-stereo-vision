#include "xil_printf.h"
#include <string.h>
#include "vdma.h"
#include "display_demo.h"

u32 vdma_version(XAxiVdma *Vdma) {
	return XAxiVdma_GetVersion(Vdma);
}

int vdma_read_start(XAxiVdma *Vdma) {
	int Status;

	// MM2S Startup
	Status = XAxiVdma_DmaStart(Vdma, XAXIVDMA_READ);
	if (Status != XST_SUCCESS)
	{
	   xil_printf("Start read transfer failed %d\n\r", Status);
	   return XST_FAILURE;
	}

	return XST_SUCCESS;
}


int vdma_read_stop(XAxiVdma *Vdma) {
	XAxiVdma_DmaStop(Vdma, XAXIVDMA_READ);
	return XST_SUCCESS;
}


int vdma_read_init(short DeviceID,short HoriSizeInput,short VertSizeInput,short Stride,unsigned int FrameStoreStartAddr)
{
	XAxiVdma Vdma;
	XAxiVdma_Config *Config;
	XAxiVdma_DmaSetup ReadCfg;
	int Status;


	Config = XAxiVdma_LookupConfig(DeviceID);
	if (NULL == Config) {
		xil_printf("XAxiVdma_LookupConfig failure\r\n");
		return XST_FAILURE;
	}

	Status = XAxiVdma_CfgInitialize(&Vdma, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("XAxiVdma_CfgInitialize failure\r\n");
		return XST_FAILURE;
	}



	ReadCfg.EnableCircularBuf = 1;
	ReadCfg.EnableFrameCounter = 0;
	ReadCfg.FixedFrameStoreAddr = 0;

	ReadCfg.EnableSync = 0;
	ReadCfg.PointNum = 1;

	ReadCfg.FrameDelay = 0;

	ReadCfg.VertSizeInput = VertSizeInput;
	ReadCfg.HoriSizeInput = HoriSizeInput;
	ReadCfg.Stride = Stride;

	Status = XAxiVdma_DmaConfig(&Vdma, XAXIVDMA_READ, &ReadCfg);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,
				"Read channel config failed %d\r\n", Status);

			return XST_FAILURE;
	}


	ReadCfg.FrameStoreStartAddr[0] = FrameStoreStartAddr;

	Status = XAxiVdma_DmaSetBufferAddr(&Vdma, XAXIVDMA_READ, ReadCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,"Read channel set buffer address failed %d\r\n", Status);
			return XST_FAILURE;
	}


	Status = vdma_read_start(&Vdma);
	if (Status != XST_SUCCESS) {
		   xil_printf("error starting VDMA..!");
		   return Status;
	}
	return XST_SUCCESS;

}


int vdma_write_start(XAxiVdma *Vdma) {
	int Status;

	// MM2S Startup
	Status = XAxiVdma_DmaStart(Vdma, XAXIVDMA_WRITE);
	if (Status != XST_SUCCESS)
	{
	   xil_printf("Start write transfer failed %d\n\r", Status);
	   return XST_FAILURE;
	}

	return XST_SUCCESS;
}


int vdma_write_stop(XAxiVdma *Vdma) {
	XAxiVdma_DmaStop(Vdma, XAXIVDMA_WRITE);
	return XST_SUCCESS;
}


int vdma_write_init(XAxiVdma *Vdma,short DeviceID,short HoriSizeInput,short VertSizeInput,short Stride,unsigned int FrameStoreStartAddr)
{
    XAxiVdma_Config *Config;
    XAxiVdma_DmaSetup WriteCfg;
    int Status;

    memset(&WriteCfg, 0, sizeof(WriteCfg));

    Config = XAxiVdma_LookupConfig(DeviceID);
    if (NULL == Config) {
        xil_printf("XAxiVdma_LookupConfig failure\r\n");
        return XST_FAILURE;
    }
    else
        xil_printf("XAxiVdma_LookupConfig ok\r\n");

    Status = XAxiVdma_CfgInitialize(Vdma, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("XAxiVdma_CfgInitialize failure\r\n");
        return XST_FAILURE;
    }
    else
        xil_printf("XAxiVdma_CfgInitialize ok\r\n");



    WriteCfg.EnableCircularBuf = 1;
    WriteCfg.EnableFrameCounter = 0;
    WriteCfg.FixedFrameStoreAddr = 0;

    WriteCfg.EnableSync = 0;
    WriteCfg.PointNum = 0;

    WriteCfg.FrameDelay = 0;

    WriteCfg.VertSizeInput = VertSizeInput;
    WriteCfg.HoriSizeInput = HoriSizeInput;
    WriteCfg.Stride = Stride;

    Status = XAxiVdma_DmaConfig(Vdma, XAXIVDMA_WRITE, &WriteCfg);
    if (Status != XST_SUCCESS) {
        xil_printf(
                "Write channel config failed %d\r\n", Status);
            return XST_FAILURE;
    }
    else xil_printf(
            "Write channel config ok %d\r\n", Status);

    WriteCfg.FrameStoreStartAddr[0] = FrameStoreStartAddr;
    WriteCfg.FrameStoreStartAddr[1] = FrameStoreStartAddr + DEMO_MAX_FRAME;
    WriteCfg.FrameStoreStartAddr[2] = FrameStoreStartAddr + 2 * DEMO_MAX_FRAME;

	Status = XAxiVdma_DmaSetBufferAddr(Vdma, XAXIVDMA_WRITE, WriteCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,"Write channel set buffer address failed %d\r\n", Status);
			return XST_FAILURE;
	}
	else
		xil_printf("Write channel set buffer address ok\r\n");


	Status = vdma_write_start(Vdma);
	if (Status != XST_SUCCESS) {
		   xil_printf("error starting VDMA..!");
		   return Status;
	}
	else
		xil_printf("VDMA start ok\r\n");

	return XST_SUCCESS;

}

/*
 * ONE-SHOT S2MM CAPTURE FUNCTION
 * -------------------------------
 * This function does not replace or modify vdma_write_init().  It is used
 * only by the DDR frame-export experiment.
 *
 * All VDMA frame-store slots point at the same requested DDR buffer and the
 * hardware frame counter is set to one.  S2MM therefore stops itself only
 * after one complete AXI4-Stream video frame has been written.
 */
int vdma_write_one_frame_init(XAxiVdma *Vdma, short DeviceID,
		short HoriSizeInput, short VertSizeInput, short Stride,
		unsigned int FrameStoreStartAddr)
{
	XAxiVdma_Config *Config;
	XAxiVdma_DmaSetup WriteCfg;
	XAxiVdma_FrameCounter FrameCounterCfg;
	int Status;
	int i;

	memset(&WriteCfg, 0, sizeof(WriteCfg));
	memset(&FrameCounterCfg, 0, sizeof(FrameCounterCfg));

	Config = XAxiVdma_LookupConfig(DeviceID);
	if (Config == NULL) {
		xil_printf("One-frame VDMA lookup failed for ID %d\r\n", DeviceID);
		return XST_FAILURE;
	}

	Status = XAxiVdma_CfgInitialize(Vdma, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS)
		return Status;

	WriteCfg.EnableCircularBuf = 1;
	WriteCfg.EnableFrameCounter = 1;
	WriteCfg.FixedFrameStoreAddr = 0;
	WriteCfg.EnableSync = 0;
	WriteCfg.PointNum = 0;
	WriteCfg.FrameDelay = 0;
	WriteCfg.VertSizeInput = VertSizeInput;
	WriteCfg.HoriSizeInput = HoriSizeInput;
	WriteCfg.Stride = Stride;

	Status = XAxiVdma_DmaConfig(Vdma, XAXIVDMA_WRITE, &WriteCfg);
	if (Status != XST_SUCCESS)
		return Status;

	for (i = 0; i < Config->MaxFrameStoreNum; i++)
		WriteCfg.FrameStoreStartAddr[i] = FrameStoreStartAddr;

	Status = XAxiVdma_DmaSetBufferAddr(Vdma, XAXIVDMA_WRITE,
			WriteCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS)
		return Status;

	/* SetFrameCounter validates both fields even though these VDMAs are S2MM-only. */
	FrameCounterCfg.ReadFrameCount = 1;
	FrameCounterCfg.ReadDelayTimerCount = 0;
	FrameCounterCfg.WriteFrameCount = 1;
	FrameCounterCfg.WriteDelayTimerCount = 0;

	Status = XAxiVdma_SetFrameCounter(Vdma, &FrameCounterCfg);
	if (Status != XST_SUCCESS)
		return Status;

	return XAxiVdma_DmaStart(Vdma, XAXIVDMA_WRITE);
}






























