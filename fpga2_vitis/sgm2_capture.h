/* Included by display_demo.c after its display/switch helpers. One frame per PL reset. */
#include "sgm2.h"

static u32 sgm2_mmio_read(void *ctx, u32 offset)
{ return Xil_In32((UINTPTR)ctx + offset); }
static void sgm2_mmio_write(void *ctx, u32 offset, u32 value)
{ Xil_Out32((UINTPTR)ctx + offset, value); }
static const sgm2_device sgm2_hw = {
    (void *)(UINTPTR)STEREO_MATCH_BASEADDR, sgm2_mmio_read, sgm2_mmio_write
};

static void sgm2_dump(DisplayCtrl *display)
{
    unsigned i;
    xil_printf("SGM2 status=%08X errors=%08X ref=%08X search=%08X pixels=%d\r\n",
        sgm2_mmio_read(sgm2_hw.context,SGM2_STATUS),
        sgm2_mmio_read(sgm2_hw.context,SGM2_ERRORS),
        sgm2_mmio_read(sgm2_hw.context,SGM2_REFERENCE_ERRORS),
        sgm2_mmio_read(sgm2_hw.context,SGM2_SEARCH_ERRORS),
        sgm2_mmio_read(sgm2_hw.context,SGM2_PIXELS));
    for(i=0;i<2;i++)
        xil_printf("Input VDMA%d CR=%08X SR=%08X\r\n",i+2,
            XAxiVdma_ReadReg(display->vdma[i]->BaseAddr,XAXIVDMA_CR_OFFSET),
            XAxiVdma_ReadReg(display->vdma[i]->BaseAddr,XAXIVDMA_SR_OFFSET));
}

static int sgm2_preflight(DisplayCtrl *display)
{
    unsigned i;
    u32 id=sgm2_mmio_read(sgm2_hw.context,SGM2_ID);
    u32 build=sgm2_mmio_read(sgm2_hw.context,SGM2_BUILD);
    u32 status=sgm2_mmio_read(sgm2_hw.context,SGM2_STATUS);
    xil_printf("SGM2 ID=%08X build=%08X status=%08X\r\n",id,build,status);
    if(id!=0x53474D32U || build!=0x20260919U ||
       sgm2_mmio_read(sgm2_hw.context,SGM2_SIZE)!=((720U<<16)|1280U) ||
#if SGM2_CONTINUOUS
       !(status&SGM2_READY) || (status&~(SGM2_READY|SGM2_DONE)) ||
       sgm2_mmio_read(sgm2_hw.context,SGM2_ERRORS)!=0U
#else
       status!=SGM2_READY || sgm2_mmio_read(sgm2_hw.context,SGM2_PIXELS)!=0U
#endif
       ) {
        xil_printf("STOP: need fresh SGM2 PL configuration. CPU-only restart is unsupported.\r\n");
        return XST_FAILURE;
    }
    for(i=0;i<2;i++) {
        if(!(XAxiVdma_ReadReg(display->vdma[i]->BaseAddr,XAXIVDMA_SR_OFFSET)&XAXIVDMA_SR_HALTED_MASK)) {
            xil_printf("STOP: input MM2S already running; no local reset attempted.\r\n");
            return XST_FAILURE;
        }
    }
    return XST_SUCCESS;
}

/* DmaConfig/SetBufferAddr do not write VSIZE: DmaStart releases the frame later. */
static int sgm2_prepare_input(XAxiVdma *v, u32 address)
{
    XAxiVdma_DmaSetup cfg;
    XAxiVdma_FrameCounter counter;
    unsigned i;
    int rc;
    memset(&cfg,0,sizeof(cfg));
    memset(&counter,0,sizeof(counter));
    cfg.VertSizeInput=720; cfg.HoriSizeInput=1280*4; cfg.Stride=1920*4;
    cfg.EnableCircularBuf=1; cfg.EnableFrameCounter=1; cfg.EnableSync=0;
    for(i=0;i<DISPLAY_NUM_FRAMES;i++) cfg.FrameStoreStartAddr[i]=address;
    counter.ReadFrameCount=1; counter.WriteFrameCount=1;
    rc=XAxiVdma_DmaConfig(v,XAXIVDMA_READ,&cfg);
    if(rc!=XST_SUCCESS) return rc;
    rc=XAxiVdma_DmaSetBufferAddr(v,XAXIVDMA_READ,cfg.FrameStoreStartAddr);
    if(rc!=XST_SUCCESS) return rc;
    return XAxiVdma_SetFrameCounter(v,&counter);
}

static int sgm2_capture_once(DisplayCtrl *display, XAxiVdma *out,
                             u8 *left, u8 *right, u32 destination)
{
    XTime begin,now;
    sgm2_result result;
    unsigned i;
    u32 elapsed, hist[256]={0}, malformed=0;
    #if !SGM2_FROZEN_OVERLAY
    u32 frames[DISPARITY_FRAME_COUNT]={destination,destination,destination};
    #endif
    int rc;
    if(sgm2_preflight(display)!=XST_SUCCESS) return XST_FAILURE;
    /* CPU must own no dirty cache lines when PL writes this buffer. */
    memset((void *)(UINTPTR)destination,0xA5,DISPARITY_FRAME_BYTES);
    Xil_DCacheFlushRange(destination,DISPARITY_FRAME_BYTES);
    /* Both remap/XSCT inputs are immutable now. Discard CPU stale copies. */
    Xil_DCacheInvalidateRange((INTPTR)left,RECTIFY_OUTPUT_BYTES);
    Xil_DCacheInvalidateRange((INTPTR)right,RECTIFY_OUTPUT_BYTES);
    if(sgm2_prepare_input(display->vdma[0],(u32)(UINTPTR)left)!=XST_SUCCESS ||
       sgm2_prepare_input(display->vdma[1],(u32)(UINTPTR)right)!=XST_SUCCESS)
        return XST_FAILURE;
    /* Existing physical wiring: VDMA2 -> LEFT, VDMA3 -> RIGHT.
     * Packaged shell selects reference=RIGHT, search=LEFT internally. */
#if SGM2_CONTINUOUS
    {static unsigned routed;if(!routed){axis_switch_select_stereo_path();routed=1;}}
#else
    axis_switch_select_stereo_path();
#endif
    XTime_GetTime(&begin);
    rc=sgm2_start(&sgm2_hw,destination,DISPARITY_STRIDE,1280,720);
    if(rc!=SGM2_OK) goto failed;
    rc=XAxiVdma_DmaStart(display->vdma[0],XAXIVDMA_READ);
    if(rc!=XST_SUCCESS) goto failed;
    rc=XAxiVdma_DmaStart(display->vdma[1],XAXIVDMA_READ);
    if(rc!=XST_SUCCESS) goto failed;
    for(;;) {
        rc=sgm2_poll(&sgm2_hw,&result);
        if(rc!=SGM2_PENDING) break;
        for(i=0;i<2;i++)
            if(XAxiVdma_GetDmaChannelErrors(display->vdma[i],XAXIVDMA_READ)) {
                rc=XST_DMA_ERROR; goto failed;
            }
        XTime_GetTime(&now);
        if(remap_elapsed_us(begin,now)>=10000000U) {rc=-100; goto failed;}
        usleep(100);
    }
    if(rc!=SGM2_OK) goto failed;
    /* Frame counter must stop both sources, with no second frame queued. */
    for(i=0;i<2;i++) {
        while(!(XAxiVdma_ReadReg(display->vdma[i]->BaseAddr,XAXIVDMA_SR_OFFSET)&XAXIVDMA_SR_HALTED_MASK)) {
            XTime_GetTime(&now);
            if(remap_elapsed_us(begin,now)>=10000000U) {rc=-101; goto failed;}
            usleep(100);
        }
        if(XAxiVdma_GetDmaChannelErrors(display->vdma[i],XAXIVDMA_READ)) {
            rc=XST_DMA_ERROR; goto failed;
        }
    }
    XTime_GetTime(&now); elapsed=remap_elapsed_us(begin,now);
    Xil_DCacheInvalidateRange(destination,DISPARITY_FRAME_BYTES);
    for(i=0;i<1280U*720U;i++) {
        u16 q=((const u16 *)(UINTPTR)destination)[i];
        if(q&0xF00FU) malformed++;
        hist[(q>>4)&255U]++;
    }
    if(malformed) {rc=-102; goto failed;}
    xil_printf("SGM2 transfer PASS: pixels=%d time=%d us; Q4 layout checked.\r\n",result.pixels,elapsed);
    xil_printf("Histogram endpoints: d=0:%d d=255:%d (neither is a defined invalid value).\r\n",hist[0],hist[255]);
    xil_printf("Accuracy NOT yet verified: export raw right-reference Q4 and compare.\r\n");
    xil_printf("mrd -force -size w -bin -file E:/vitis_workspace_dual_camera/sgm2_disparity_q4.bin 0x%08X 0x70800\r\n",destination);
    xil_printf("mrd -force -size w -bin -file E:/vitis_workspace_dual_camera/sgm2_left.bin 0x%08X 0x151800\r\n",(u32)(UINTPTR)left);
    xil_printf("mrd -force -size w -bin -file E:/vitis_workspace_dual_camera/sgm2_right.bin 0x%08X 0x151800\r\n",(u32)(UINTPTR)right);
    #if SGM2_FROZEN_OVERLAY
    #if OVERLAY_DEBUG_HANDOFF
    return overlay_wait_and_publish(display,right,destination);
    #else
    return overlay_hardware_and_publish(display,right,destination);
    #endif
    #else
    /* VDMA4 first starts only after DDR completion and checks. */
    rc=disparity_vdma_initialize(out,frames);
    if(rc!=XST_SUCCESS) goto failed;
    xil_printf("Frozen SGM2 frame on HDMI: gray=d (0..255), not metric depth.\r\n");
    return XST_SUCCESS;
    #endif
failed:
    xil_printf("SGM2 STOP rc=%d. No retry or local AXI reset; reconfigure PL before rerun.\r\n",rc);
    sgm2_dump(display);
    return XST_FAILURE;
}

#if SGM2_FIXED_REPLAY
static volatile u32 sgm2_replay_command[16] __attribute__((aligned(64)));
static int sgm2_replay(DisplayCtrl *display,XAxiVdma *out,u8 *left,u8 *right,u32 dest)
{
    if(sgm2_preflight(display)!=XST_SUCCESS) return XST_FAILURE;
    sgm2_replay_command[0]=0;
    Xil_DCacheFlushRange((INTPTR)sgm2_replay_command,sizeof(sgm2_replay_command));
    /* Flush any initialized image lines BEFORE debugger writes DDR. */
    Xil_DCacheFlushRange((INTPTR)left,RECTIFY_OUTPUT_BYTES);
    Xil_DCacheFlushRange((INTPTR)right,RECTIFY_OUTPUT_BYTES);
    xil_printf("SGM2 fixed replay: load two 5529600-byte BGRX files, stride 7680.\r\n");
    xil_printf("stop\r\ndow -data E:/vitis_workspace_dual_camera/sgm2_input_left.bin 0x%08X\r\n",(u32)(UINTPTR)left);
    xil_printf("dow -data E:/vitis_workspace_dual_camera/sgm2_input_right.bin 0x%08X\r\n",(u32)(UINTPTR)right);
    xil_printf("mwr 0x%08X 1\r\ncon\r\n",(u32)(UINTPTR)sgm2_replay_command);
    for(;;) {
        Xil_DCacheInvalidateRange((INTPTR)sgm2_replay_command,sizeof(sgm2_replay_command));
        if(sgm2_replay_command[0]==1U) break;
        usleep(10000);
    }
    return sgm2_capture_once(display,out,left,right,dest);
}
#endif
