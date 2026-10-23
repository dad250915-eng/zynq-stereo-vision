/* Included after frozen overlay storage. Requires continuous RGB565 BD. */
#define STREAM_RGB0 0x08000000U
#define STREAM_RGB1 0x08200000U
#define STREAM_RAW_LEFT 0x08400000U
#define STREAM_RAW_RIGHT 0x08A00000U
#define STREAM_RGB_BYTES (1280U*720U*2U)
#if XPAR_PS7_DDR_0_S_AXI_HIGHADDR < 0x08F46000U
#error Continuous buffers exceed DDR
#endif
static XAxiVdma stream_display_vdma;
static unsigned stream_display_started,stream_front;
/* 0 run, 1 request pause, 2 paused at completed-frame boundary. */
volatile u32 stream_command[16] __attribute__((aligned(64)));
static int stream_present_rgb(unsigned slot)
{
    u32 bases[3]={STREAM_RGB0,STREAM_RGB1,STREAM_RGB0};
    XTime begin,now;int rc;
    if(slot>1U || (stream_display_started && slot==stream_front))return XST_FAILURE;
    if(!stream_display_started) {
        XAxiVdma_Config *check=XAxiVdma_LookupConfig(DISPARITY_VDMA_DEVICE_ID);
        if(!check || !(XAxiVdma_ReadReg(check->BaseAddress,XAXIVDMA_SR_OFFSET)&XAXIVDMA_SR_HALTED_MASK))
            return XST_FAILURE; /* Refuse CPU-only restart while HDMI owns buffers. */
    }
    if(!stream_display_started) {
        XAxiVdma_Config *vc=XAxiVdma_LookupConfig(DISPARITY_VDMA_DEVICE_ID);
        XAxiVdma_DmaSetup cfg;
        if(!vc)return XST_FAILURE;
        rc=XAxiVdma_CfgInitialize(&stream_display_vdma,vc,vc->BaseAddress);if(rc)return rc;
        memset(&cfg,0,sizeof(cfg));cfg.VertSizeInput=720;cfg.HoriSizeInput=2560;
        cfg.Stride=2560;cfg.EnableCircularBuf=1;
        memcpy(cfg.FrameStoreStartAddr,bases,sizeof(bases));
        rc=XAxiVdma_DmaConfig(&stream_display_vdma,XAXIVDMA_READ,&cfg);if(rc)return rc;
        rc=XAxiVdma_DmaSetBufferAddr(&stream_display_vdma,XAXIVDMA_READ,cfg.FrameStoreStartAddr);if(rc)return rc;
        /* Initial circulation is safe: all slots hold the same completed image. */
        if(slot!=0U)return XST_FAILURE;
        Xil_DCacheInvalidateRange(STREAM_RGB0,STREAM_RGB_BYTES);
        memcpy((void *)(UINTPTR)STREAM_RGB1,(void *)(UINTPTR)STREAM_RGB0,STREAM_RGB_BYTES);
        Xil_DCacheFlushRange(STREAM_RGB1,STREAM_RGB_BYTES);
        rc=XAxiVdma_DmaStart(&stream_display_vdma,XAXIVDMA_READ);if(rc)return rc;
        rc=XAxiVdma_StartParking(&stream_display_vdma,slot,XAXIVDMA_READ);if(rc)return rc;
        stream_display_started=1;
    } else {
        rc=XAxiVdma_StartParking(&stream_display_vdma,slot,XAXIVDMA_READ);if(rc)return rc;
    }
    XTime_GetTime(&begin);
    while(XAxiVdma_CurrFrameStore(&stream_display_vdma,XAXIVDMA_READ)!=slot) {
        if(XAxiVdma_GetDmaChannelErrors(&stream_display_vdma,XAXIVDMA_READ))return XST_FAILURE;
        XTime_GetTime(&now);if(remap_elapsed_us(begin,now)>1000000U)return XST_FAILURE;
        usleep(100);
    }
    if(XAxiVdma_GetDmaChannelErrors(&stream_display_vdma,XAXIVDMA_READ))return XST_FAILURE;
    /* Hardware frame pointer acknowledges the new frame; old buffer can now be reused. */
    stream_front=slot;
    return XST_SUCCESS;
}
static int stream_present(const u8 *bgrx)
{
    unsigned x,y,slot=stream_display_started?(stream_front^1U):0;
    u16 *dst=(u16 *)(UINTPTR)(slot?STREAM_RGB1:STREAM_RGB0);
    for(y=0;y<720;y++)for(x=0;x<1280;x++) {
        const u8 *p=bgrx+y*7680+(x+320)*4;
        dst[y*1280+x]=(u16)(((p[2]>>3)<<11)|((p[1]>>2)<<5)|(p[0]>>3));
    }
    Xil_DCacheFlushRange((INTPTR)dst,STREAM_RGB_BYTES);
    return stream_present_rgb(slot);
}
static void stream_pause_boundary(void)
{
    Xil_DCacheInvalidateRange((INTPTR)stream_command,sizeof(stream_command));
    if(stream_command[0]==1) {
        stream_command[0]=2;Xil_DCacheFlushRange((INTPTR)stream_command,sizeof(stream_command));
        xil_printf("STREAM PAUSED: descriptor=%08X command=%08X. Write command=0 to resume.\r\n",
            (u32)(UINTPTR)overlay_request,(u32)(UINTPTR)stream_command);
        do {
            usleep(10000);
            Xil_DCacheInvalidateRange((INTPTR)stream_command,sizeof(stream_command));
        } while(stream_command[0]!=0);
    }
}
/* DIRECT_RAW_CAPTURE_BEGIN
 * Park each S2MM writer on a DIFFERENT slot at a frame boundary. The slot
 * being written before parking then becomes a completed, immutable input.
 * The parked slot continues accepting incoming video while remap reads the
 * protected slot. Release only after BOTH remap generators/consumers finish.
 * This protects buffer ownership; it does not synchronize camera exposures.
 */
#ifndef STREAM_CAPTURE_PARK_TIMEOUT_US
#define STREAM_CAPTURE_PARK_TIMEOUT_US 60000000U
#endif
static XAxiVdma *stream_capture_vdma[2];
static unsigned stream_capture_requested,stream_capture_active;
static u32 stream_capture_park_slot[2];
static void stream_capture_release(void)
{
    unsigned i;
    for(i=0;i<2;i++)if(stream_capture_requested&(1U<<i))
        XAxiVdma_StopParking(stream_capture_vdma[i],XAXIVDMA_WRITE);
    stream_capture_requested=stream_capture_active=0;
}
static int stream_capture_health(XAxiVdma *v)
{
    u32 control,status,errors;
    if(!v || !v->WriteChannel.IsValid || v->MaxNumFrames!=DISPLAY_NUM_FRAMES)
        return XST_FAILURE;
    control=XAxiVdma_ReadReg(v->WriteChannel.ChanBase,XAXIVDMA_CR_OFFSET);
    status=XAxiVdma_ReadReg(v->WriteChannel.ChanBase,XAXIVDMA_SR_OFFSET);
    errors=XAxiVdma_GetDmaChannelErrors(v,XAXIVDMA_WRITE);
    return (control&XAXIVDMA_CR_RUNSTOP_MASK) &&
           !(status&XAXIVDMA_SR_HALTED_MASK) && !errors ? XST_SUCCESS:XST_FAILURE;
}
static int stream_capture_protected(void)
{
    unsigned i;
    if(!stream_capture_active || (rx_status()&31U)!=27U)return XST_FAILURE;
    for(i=0;i<2;i++){
        XAxiVdma *v=stream_capture_vdma[i];
        u32 cr=XAxiVdma_ReadReg(v->WriteChannel.ChanBase,XAXIVDMA_CR_OFFSET);
        u32 ptr=XAxiVdma_ReadReg(v->BaseAddr,XAXIVDMA_PARKPTR_OFFSET);
        if(stream_capture_health(v)!=XST_SUCCESS || (cr&XAXIVDMA_CR_TAIL_EN_MASK) ||
           ((ptr&XAXIVDMA_PARKPTR_WRTREF_MASK)>>XAXIVDMA_WRTREF_SHIFT)!=stream_capture_park_slot[i] ||
           XAxiVdma_CurrFrameStore(v,XAXIVDMA_WRITE)!=stream_capture_park_slot[i])
            return XST_FAILURE;
    }
    return XST_SUCCESS;
}
static int stream_capture_hold(XAxiVdma **c,u32 source_slot[2],u32 *wait_us)
{
    unsigned i;XTime begin,now;
    if(stream_capture_active || stream_capture_requested)return XST_FAILURE;
    XTime_GetTime(&begin);
    *wait_us=0;
    for(i=0;i<2;i++){
        if(stream_capture_health(c[i])!=XST_SUCCESS)goto failed;
        source_slot[i]=XAxiVdma_CurrFrameStore(c[i],XAXIVDMA_WRITE);
        if(source_slot[i]>=DISPLAY_NUM_FRAMES)goto failed;
        stream_capture_vdma[i]=c[i];
        stream_capture_park_slot[i]=(source_slot[i]+1U)%DISPLAY_NUM_FRAMES;
    }
    for(i=0;i<2;i++){
        /* Track before the API call so a partially applied failure is restored. */
        stream_capture_requested|=1U<<i;
        if(XAxiVdma_StartParking(c[i],stream_capture_park_slot[i],XAXIVDMA_WRITE)!=XST_SUCCESS)
            goto failed;
    }
    for(;;){
        unsigned ready=1;
        if((rx_status()&31U)!=27U)goto failed;
        for(i=0;i<2;i++){
            u32 cr=XAxiVdma_ReadReg(c[i]->WriteChannel.ChanBase,XAXIVDMA_CR_OFFSET);
            u32 ptr=XAxiVdma_ReadReg(c[i]->BaseAddr,XAXIVDMA_PARKPTR_OFFSET);
            if(stream_capture_health(c[i])!=XST_SUCCESS || (cr&XAXIVDMA_CR_TAIL_EN_MASK) ||
               ((ptr&XAXIVDMA_PARKPTR_WRTREF_MASK)>>XAXIVDMA_WRTREF_SHIFT)!=stream_capture_park_slot[i])goto failed;
            if(XAxiVdma_CurrFrameStore(c[i],XAXIVDMA_WRITE)!=stream_capture_park_slot[i])ready=0;
        }
        XTime_GetTime(&now);
        if(ready){
            stream_capture_active=1;
            if(stream_capture_protected()!=XST_SUCCESS)goto failed;
            *wait_us=remap_elapsed_us(begin,now);
            return XST_SUCCESS;
        }
        if(remap_elapsed_us(begin,now)>=STREAM_CAPTURE_PARK_TIMEOUT_US)goto failed;
        usleep(100U);
    }
failed:
    XTime_GetTime(&now);*wait_us=remap_elapsed_us(begin,now);
    xil_printf("CAPTURE STOP: cannot protect raw buffers; wait_us=%d RX=%08X\r\n",*wait_us,rx_status());
    stream_capture_release();
    return XST_FAILURE;
}
/* DIRECT_RAW_CAPTURE_END */

/* Legacy non-ROI snapshot path only. Current ROI uses protected raw input. */
/* Copy from a camera-owned completed slot to private snapshots. Reject the
 * entire pair if either writer advances during copying (conservative).
 * Poll per row; camera period must exceed one row copy + register polling.
 * This detects buffer reuse, not exposure synchronization. */
static int stream_snapshot(XAxiVdma **c,const u32 *current,const u32 *source)
{
    unsigned y,i;u32 dst[2]={STREAM_RAW_RIGHT,STREAM_RAW_LEFT};
    for(i=0;i<2;i++) {
        Xil_DCacheInvalidateRange(source[i],RECTIFY_OUTPUT_BYTES);
        for(y=0;y<720;y++) {
            if(XAxiVdma_CurrFrameStore(c[0],XAXIVDMA_WRITE)!=current[0] ||
               XAxiVdma_CurrFrameStore(c[1],XAXIVDMA_WRITE)!=current[1])return 1;
            memcpy((void *)(UINTPTR)(dst[i]+y*7680),(const void *)(UINTPTR)(source[i]+y*7680),5120);
        }
    }
    if(XAxiVdma_CurrFrameStore(c[0],XAXIVDMA_WRITE)!=current[0] ||
       XAxiVdma_CurrFrameStore(c[1],XAXIVDMA_WRITE)!=current[1])return 1;
    for(i=0;i<2;i++)Xil_DCacheFlushRange(dst[i],RECTIFY_OUTPUT_BYTES);
    return 0;
}
