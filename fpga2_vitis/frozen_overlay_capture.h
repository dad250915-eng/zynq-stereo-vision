/* Included after AXIS routing helpers, before sgm2_capture.h. */
#include "frozen_overlay.h"
static u8 overlay_mask[1280U*720U] __attribute__((aligned(64)));
static u32 overlay_depth_result[32] __attribute__((aligned(64)));
static union {
    u8 image[1920U*720U*4U];
    u32 queue[1280U*720U];
} overlay_storage __attribute__((aligned(64)));
#define overlay_display overlay_storage.image
/* Debugger handoff descriptor, one cache line. See FROZEN_OVERLAY.md. */
volatile u32 overlay_request[16] __attribute__((aligned(64)));

#if SGM2_CONTINUOUS
#include "stream_display.h"
#endif
#include "xyz_diagnostic_capture.h"

static int overlay_start_hdmi(DisplayCtrl *display)
{
#if SGM2_CONTINUOUS
    (void)display;return stream_present(overlay_display);
#else
    XAxiVdma_DmaSetup cfg;
    unsigned i,j;
    int rc;
    /* Existing OSD takes x=320..959 of both inputs. Our picture starts at
     * column320 in a 1920-pixel row; lane1 starts 640 pixels later. Thus OSD
     * emits picture x=0..639 then x=640..1279 with no crop/rescale. */
    for(i=0;i<2;i++) {
        if(!(XAxiVdma_ReadReg(display->vdma[i]->BaseAddr,XAXIVDMA_SR_OFFSET)&XAXIVDMA_SR_HALTED_MASK))
            return XST_FAILURE;
        if(XAxiVdma_GetDmaChannelErrors(display->vdma[i],XAXIVDMA_READ))return XST_FAILURE;
        memset(&cfg,0,sizeof(cfg));
        cfg.VertSizeInput=720;cfg.HoriSizeInput=1280*4;cfg.Stride=1920*4;
        cfg.EnableCircularBuf=1;cfg.EnableFrameCounter=0;cfg.EnableSync=0;
        for(j=0;j<DISPLAY_NUM_FRAMES;j++)
            cfg.FrameStoreStartAddr[j]=(u32)(UINTPTR)overlay_display+i*640U*4U;
        rc=XAxiVdma_DmaConfig(display->vdma[i],XAXIVDMA_READ,&cfg);
        if(rc!=XST_SUCCESS)return rc;
        rc=XAxiVdma_DmaSetBufferAddr(display->vdma[i],XAXIVDMA_READ,cfg.FrameStoreStartAddr);
        if(rc!=XST_SUCCESS)return rc;
    }
    /* Both SGM input sources have halted and OSD has never started this run.
     * Route only now; do not reset a live AXI master or start VDMA4. */
    axis_switch_select_binocular_path();
    for(i=0;i<2;i++) {
        rc=XAxiVdma_DmaStart(display->vdma[i],XAXIVDMA_READ);
        if(rc!=XST_SUCCESS)return rc;
        rc=XAxiVdma_StartParking(display->vdma[i],0,XAXIVDMA_READ);
        if(rc!=XST_SUCCESS)return rc;
    }
    return XST_SUCCESS;
#endif
}

/* Common consumer for future hardware producer and current XSCT handoff.
 * Producer must finish DDR writes before calling/committing. All buffers
 * remain immutable once display starts. No automatic retries or new frames. */
static int overlay_publish(DisplayCtrl *display,const u8 *right,u32 frame_id)
{
    int rc;
    Xil_DCacheInvalidateRange((INTPTR)overlay_mask,sizeof(overlay_mask));
    Xil_DCacheInvalidateRange((INTPTR)overlay_depth_result,sizeof(overlay_depth_result));
    Xil_DCacheInvalidateRange((INTPTR)right,RECTIFY_OUTPUT_BYTES);
    rc=fo_render(right,7680,overlay_mask,1280,overlay_depth_result,
                 1280,720,frame_id,overlay_display,7680);
    if(rc!=FO_OK) {
        xyz_state(frame_id,XYZ_BAD_METADATA);
        xil_printf("OVERLAY rejected: rc=%d; no old depth displayed.\r\n",rc);
        return XST_FAILURE;
    }
    xyz_diagnostic_publish(frame_id);
    Xil_DCacheFlushRange((INTPTR)overlay_display,sizeof(overlay_display));
    rc=overlay_start_hdmi(display);
    if(rc!=XST_SUCCESS)return rc;
    xil_printf("Frozen RIGHT color + red mask contour/bbox + Z on HDMI.\r\n");
    xil_printf("Depth status=%d valid=%d selected=%d accepted=%d\r\n",
        overlay_depth_result[2],overlay_depth_result[15],overlay_depth_result[5],overlay_depth_result[6]);
    xil_printf("Overlay BGRX DDR=0x%08X stride=7680 active_x_offset=1280 bytes\r\n",(u32)(UINTPTR)overlay_display);
    return XST_SUCCESS;
}

#if !OVERLAY_DEBUG_HANDOFF
#include "overlay_ip_capture.h"
#else
static int overlay_wait_and_publish(DisplayCtrl *display,const u8 *right,u32 disparity)
{
    unsigned i;int rc;XTime stamp;u32 frame_id;
    /* This BSP has no color/depth IP. Explicit debugger handoff; no guessed
     * MMIO addresses and no silent CPU replacement of the two hardware IPs. */
    memset(overlay_mask,0,sizeof(overlay_mask));
    memset(overlay_depth_result,0,sizeof(overlay_depth_result));
    Xil_DCacheFlushRange((INTPTR)overlay_mask,sizeof(overlay_mask));
    Xil_DCacheFlushRange((INTPTR)overlay_depth_result,sizeof(overlay_depth_result));
    for(i=0;i<16;i++)overlay_request[i]=0;
    XTime_GetTime(&stamp);
    frame_id=(u32)stamp ^ (u32)(stamp>>32);
    if(!frame_id)frame_id=1;
    overlay_request[0]=0x4F564C31U;overlay_request[1]=frame_id;
    overlay_request[2]=1280;overlay_request[3]=720;
    overlay_request[4]=(u32)(UINTPTR)right;overlay_request[5]=7680;
    overlay_request[6]=disparity;overlay_request[7]=2560;
    overlay_request[8]=(u32)(UINTPTR)overlay_mask;overlay_request[9]=1280;
    overlay_request[10]=(u32)(UINTPTR)overlay_depth_result;
    overlay_request[11]=(u32)(UINTPTR)overlay_display;overlay_request[12]=7680;
    Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
    xil_printf("OVERLAY HANDOFF WAIT: frame=0x%08X descriptor=0x%08X. Debug handoff mode selected.\r\n",frame_id,(u32)(UINTPTR)overlay_request);
    xil_printf("Use scripts/overlay_xsct.tcl dump <descriptor> <folder>, then load selected mask/result.\r\n");
    /* Deliberate user-controlled debugger wait, not a hardware completion
     * poll. The SGM input VDMAs stay halted; no grayscale VDMA4 is started. */
    for(;;) {
        Xil_DCacheInvalidateRange((INTPTR)overlay_request,sizeof(overlay_request));
        if(overlay_request[15]==0x434F4D54U)break;
        usleep(10000);
    }
    overlay_request[14]=1;overlay_request[15]=0;
    Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
    rc=overlay_publish(display,right,frame_id);
    overlay_request[14]=(rc==XST_SUCCESS)?2U:0x80000000U;
    Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
    return rc;
}

#endif
