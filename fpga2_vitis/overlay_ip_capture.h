/* Included after overlay_publish. Only called after SGM and its sources halt. */
#include "xcolor_divider_hw.h"
#include "xdepth_measure_hw.h"
#include "overlay_select.h"
#include "overlay_ip_config.h"
#define OC_BASE XPAR_COLOR_DIVIDER_0_S_AXI_CONTROL_BASEADDR
#define OD_BASE XPAR_DEPTH_MEASURE_0_S_AXI_CONTROL_BASEADDR
#define CW(n,v) Xil_Out32(OC_BASE+XCOLOR_DIVIDER_CONTROL_ADDR_##n,(v))
#define CR(n) Xil_In32(OC_BASE+XCOLOR_DIVIDER_CONTROL_ADDR_##n)
#define DW(n,v) Xil_Out32(OD_BASE+XDEPTH_MEASURE_CONTROL_ADDR_##n,(v))
static XAxiVdma overlay_vdma;
/* Change in source before build, or via debugger BEFORE entering this stage.
 * Snapshot once to prevent a mid-frame mixed configuration. */
volatile u32 overlay_parameters[8] __attribute__((aligned(64)))={
    OVERLAY_HSV_LOW,OVERLAY_HSV_HIGH,OVERLAY_MIN_AREA,OVERLAY_MAX_AREA,
    OVERLAY_MIN_POINTS,OVERLAY_MIN_SUPPORT,OVERLAY_MAX_IQR,0};
static u32 overlay_float_bits(float v){u32 b;memcpy(&b,&v,4);return b;}
static u32 overlay_dma_status(unsigned dir)
{return XAxiVdma_ReadReg(overlay_vdma.BaseAddr,(dir==XAXIVDMA_READ?XAXIVDMA_TX_OFFSET:XAXIVDMA_RX_OFFSET)+XAXIVDMA_SR_OFFSET);}
/* A single frozen frame has no following SOF to advance TUSER-mode S2MM's
 * interrupt counter. Prove DDR coverage using an impossible mask value.
 * Buffer was flushed with 0xA5 before DMA and is CPU-read-only until HALTED.
 * Every byte must be overwritten; checking just the last word is insufficient.
 * Only valid for this IP's exact binary 0/255 output, not arbitrary images. */
static int overlay_mask_written(u32 *pending)
{
    unsigned i;u32 left=0;
    Xil_DCacheInvalidateRange((INTPTR)overlay_mask,sizeof(overlay_mask));
    for(i=0;i<sizeof(overlay_mask);i++) {
        u8 v=((const volatile u8*)overlay_mask)[i];
        if(v==0xA5)left++;
        else if(v!=0&&v!=255)return -1;
    }
    *pending=left;return left==0;
}
static int overlay_prepare_dma(unsigned dir,u32 address)
{
    XAxiVdma_DmaSetup c;unsigned i;int rc;
    memset(&c,0,sizeof(c));c.VertSizeInput=720;
    c.HoriSizeInput=dir==XAXIVDMA_READ?5120:1280;
    c.Stride=dir==XAXIVDMA_READ?7680:1280;
    c.EnableCircularBuf=1;c.EnableFrameCounter=1;
    for(i=0;i<3;i++)c.FrameStoreStartAddr[i]=address;
    rc=XAxiVdma_DmaConfig(&overlay_vdma,dir,&c);if(rc)return rc;
    return XAxiVdma_DmaSetBufferAddr(&overlay_vdma,dir,c.FrameStoreStartAddr);
}
static int overlay_hardware_and_publish(DisplayCtrl *display,const u8 *right,u32 disparity)
{
    XAxiVdma_Config *vc;XAxiVdma_FrameCounter fc;
    XTime begin,now;u32 frame,p[8],i,done=0,rd,wr,area,pending=921600;int rc=-1,coverage;
    xyz_state(0,XYZ_BUSY);
    /* All failure paths stop publication. Do not reset in-flight AXI masters;
     * reconfigure PL before retrying this one-shot application. */
    for(i=0;i<8;i++)p[i]=overlay_parameters[i];
    if((p[0]>>24)||(p[1]>>24)||(p[0]&255)>179||(p[1]&255)>179||
       ((p[0]>>8)&255)>((p[1]>>8)&255)||((p[0]>>16)&255)>((p[1]>>16)&255)||
       !p[2]||p[2]>p[3]||p[3]>921600||!p[4]||p[4]>921600||p[5]>1000||p[6]>255)goto fail;
    /* Preflight the actual hardware before CfgInitialize performs DMA reset. */
    rd=XAxiVdma_ReadReg(XPAR_AXI_VDMA_5_BASEADDR,XAXIVDMA_TX_OFFSET+XAXIVDMA_SR_OFFSET);
    wr=XAxiVdma_ReadReg(XPAR_AXI_VDMA_5_BASEADDR,XAXIVDMA_RX_OFFSET+XAXIVDMA_SR_OFFSET);
    if(!(rd&1)||!(wr&1)||(rd&XAXIVDMA_SR_ERR_ALL_MASK)||(wr&XAXIVDMA_SR_ERR_ALL_MASK)||
       !(CR(AP_CTRL)&4)||!(Xil_In32(OD_BASE)&4)) {rc=-2;goto fail;}
    /* Reset/reinitialize VDMA5 only after BOTH channels are HALTED and error-free.
     * This rearms the TUSER first-SOF frame-counter state for each isolated frame. */
    vc=XAxiVdma_LookupConfig(XPAR_AXI_VDMA_5_DEVICE_ID);if(!vc){rc=-3;goto fail;}
    rc=XAxiVdma_CfgInitialize(&overlay_vdma,vc,vc->BaseAddress);if(rc)goto fail;
    #if SGM2_CONTINUOUS
    XAxiVdma_IntrClear(&overlay_vdma,XAXIVDMA_IXR_ALL_MASK,XAXIVDMA_READ);
    XAxiVdma_IntrClear(&overlay_vdma,XAXIVDMA_IXR_ALL_MASK,XAXIVDMA_WRITE);
    #endif
    memset(overlay_mask,0xA5,sizeof(overlay_mask));memset(overlay_depth_result,0,sizeof(overlay_depth_result));
    Xil_DCacheFlushRange((INTPTR)overlay_mask,sizeof(overlay_mask));
    Xil_DCacheFlushRange((INTPTR)overlay_depth_result,sizeof(overlay_depth_result));
#if SGM2_CONTINUOUS
    {static u32 sequence;frame=++sequence;if(!frame)frame=++sequence;}
#else
    XTime_GetTime(&begin);frame=(u32)begin^(u32)(begin>>32);if(!frame)frame=1;
#endif
    memset((void*)overlay_request,0,sizeof(overlay_request));
    overlay_request[0]=0x4F564C31;overlay_request[1]=frame;overlay_request[2]=1280;overlay_request[3]=720;
    overlay_request[4]=(u32)(UINTPTR)right;overlay_request[5]=7680;overlay_request[6]=disparity;overlay_request[7]=2560;
    overlay_request[8]=(u32)(UINTPTR)overlay_mask;overlay_request[9]=1280;
    overlay_request[10]=(u32)(UINTPTR)overlay_depth_result;overlay_request[11]=(u32)(UINTPTR)overlay_display;overlay_request[12]=7680;
    overlay_request[14]=1;Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
    xil_printf("OVERLAY HW START frame=%08X descriptor=%08X HSV=%08X..%08X (provisional)\r\n",frame,(u32)(UINTPTR)overlay_request,p[0],p[1]);
    rc=overlay_prepare_dma(XAXIVDMA_WRITE,(u32)(UINTPTR)overlay_mask);if(rc)goto fail;
    rc=overlay_prepare_dma(XAXIVDMA_READ,(u32)(UINTPTR)right);if(rc)goto fail;
    memset(&fc,0,sizeof(fc));fc.ReadFrameCount=1;fc.WriteFrameCount=1;
    rc=XAxiVdma_SetFrameCounter(&overlay_vdma,&fc);if(rc)goto fail;
    CW(GIE,0);CW(IER,0);CW(ROWS_DATA,720);CW(COLS_DATA,1280);
    CW(HSV_LOW_DATA,p[0]);CW(HSV_HIGH_DATA,p[1]);CW(FRAME_ID_DATA,frame);
    /* Clear stale read-to-clear completion flags before this invocation. */
    (void)CR(AP_CTRL);(void)CR(RESULT_FRAME_ID_CTRL);(void)CR(STATUS_CTRL);(void)CR(OUTPUT_PIXELS_CTRL);
    rc=XAxiVdma_DmaStart(&overlay_vdma,XAXIVDMA_WRITE);if(rc)goto fail;
    CW(AP_CTRL,1);
    rc=XAxiVdma_DmaStart(&overlay_vdma,XAXIVDMA_READ);if(rc)goto fail;
    XTime_GetTime(&begin);
    for(;;) {
        done|=CR(AP_CTRL)&2;rd=overlay_dma_status(XAXIVDMA_READ);wr=overlay_dma_status(XAXIVDMA_WRITE);
        if(XAxiVdma_GetDmaChannelErrors(&overlay_vdma,XAXIVDMA_READ)||XAxiVdma_GetDmaChannelErrors(&overlay_vdma,XAXIVDMA_WRITE)){rc=-4;goto fail;}
        if(done&&(rd&1)&&(rd&XAXIVDMA_IXR_FRMCNT_MASK))break;
        XTime_GetTime(&now);if(remap_elapsed_us(begin,now)>=OVERLAY_TIMEOUT_US){rc=-5;goto fail;}
        usleep(100);
    }
    if(!(CR(RESULT_FRAME_ID_CTRL)&1)||!(CR(STATUS_CTRL)&1)||!(CR(OUTPUT_PIXELS_CTRL)&1)||
       CR(RESULT_FRAME_ID_DATA)!=frame||CR(STATUS_DATA)!=0||CR(OUTPUT_PIXELS_DATA)!=921600){rc=-6;goto fail;}
    for(;;) {
        if(XAxiVdma_GetDmaChannelErrors(&overlay_vdma,XAXIVDMA_WRITE)){rc=-4;goto fail;}
        coverage=overlay_mask_written(&pending);
        if(coverage<0){rc=-9;goto fail;}
        if(coverage)break;
        XTime_GetTime(&now);if(remap_elapsed_us(begin,now)>=OVERLAY_TIMEOUT_US){rc=-8;goto fail;}
        usleep(100);
    }
    /* All expected writes are visible and the producer is done. RS=0 is a
     * normal channel stop, NOT reset. Never stop an incompletely written frame. */
    XAxiVdma_DmaStop(&overlay_vdma,XAXIVDMA_WRITE);
    XTime_GetTime(&begin);
    while(!(overlay_dma_status(XAXIVDMA_WRITE)&1)) {
        if(XAxiVdma_GetDmaChannelErrors(&overlay_vdma,XAXIVDMA_WRITE)){rc=-4;goto fail;}
        XTime_GetTime(&now);if(remap_elapsed_us(begin,now)>=OVERLAY_TIMEOUT_US){rc=-10;goto fail;}
        usleep(100);
    }
    if(XAxiVdma_GetDmaChannelErrors(&overlay_vdma,XAXIVDMA_WRITE)){rc=-4;goto fail;}
    if(overlay_mask_written(&pending)!=1){rc=-11;goto fail;}
    xil_printf("MASK DDR PASS: 921600 binary bytes written; S2MM halted without next SOF.\r\n");
    Xil_DCacheInvalidateRange((INTPTR)overlay_mask,sizeof(overlay_mask));
    /* Reuse the not-yet-displayed output buffer as BFS scratch; no extra MB. */
    area=overlay_select(overlay_mask,1280,720,p[2],p[3],overlay_storage.queue);
    Xil_DCacheFlushRange((INTPTR)overlay_mask,sizeof(overlay_mask));
    xil_printf("COLOR PASS: selected=%d pixels (largest eligible 4-connected component).\r\n",area);
    DW(GIE,0);DW(IER,0);DW(DISPARITY_DATA,disparity);Xil_Out32(OD_BASE+0x14,0);
    DW(MASK_DATA,(u32)(UINTPTR)overlay_mask);Xil_Out32(OD_BASE+0x20,0);
    DW(RESULT_DATA,(u32)(UINTPTR)overlay_depth_result);Xil_Out32(OD_BASE+0x2c,0);
    DW(ROWS_DATA,720);DW(COLS_DATA,1280);DW(DISPARITY_STRIDE_BYTES_DATA,2560);DW(MASK_STRIDE_BYTES_DATA,1280);
    DW(ROI_X_DATA,0);DW(ROI_Y_DATA,0);DW(ROI_WIDTH_DATA,1280);DW(ROI_HEIGHT_DATA,720);
    DW(DISPARITY_FRAME_ID_DATA,frame);DW(MASK_FRAME_ID_DATA,frame);
    DW(MIN_DISPARITY_DATA,1);DW(MAX_DISPARITY_DATA,254);DW(MIN_POINTS_DATA,p[4]);DW(MIN_SUPPORT_PERMILLE_DATA,p[5]);DW(MAX_IQR_DATA,p[6]);
    DW(NUMERATOR_MM_PX_DATA,overlay_float_bits(OVERLAY_NUMERATOR));DW(PRINCIPAL_DELTA_PX_DATA,overlay_float_bits(OVERLAY_PRINCIPAL_DELTA));
    (void)Xil_In32(OD_BASE);DW(AP_CTRL,1);XTime_GetTime(&begin);
    while(!(Xil_In32(OD_BASE)&2)){
        XTime_GetTime(&now);if(remap_elapsed_us(begin,now)>=OVERLAY_TIMEOUT_US){rc=-7;goto fail;}usleep(100);
    }
    rc=overlay_publish(display,right,frame);
    overlay_request[14]=rc?0x80000000U:2U;Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
    return rc;
fail:
    xyz_state(0,XYZ_PIPELINE_STOP);
    xil_printf("MASK pending sentinel bytes=%d (last scan; initial=921600).\r\n",pending);
    xil_printf("OVERLAY HW STOP rc=%d color_ctrl=%08X color_status=%08X depth_ctrl=%08X\r\n",rc,CR(AP_CTRL),CR(STATUS_DATA),Xil_In32(OD_BASE));
    xil_printf("VDMA5 SR read=%08X write=%08X. Reconfigure PL before retry.\r\n",
        XAxiVdma_ReadReg(XPAR_AXI_VDMA_5_BASEADDR,4),XAxiVdma_ReadReg(XPAR_AXI_VDMA_5_BASEADDR,0x34));
    overlay_request[14]=0x80000000U;Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
    return XST_FAILURE;
}
#undef CW
#undef CR
#undef DW
