/* Included after display_demo.c helpers. No extra source or BSP changes. */
#ifndef STEREO_FIXED_REPLAY
#define STEREO_FIXED_REPLAY 0
#endif
#define STM_BUILD 0x34U
#define STM_INPUT_COUNT 0x38U
#define STM_SUM_L 0x3CU
#define STM_SUM_R 0x40U
#define STM_HASH_L 0x44U
#define STM_HASH_R 0x48U
#define STM_OUTPUT_HASH 0x4CU
#define STM_SOF_L 0x50U
#define STM_SOF_R 0x54U

/* Dedicated cache line: XSCT writes word 0 after BOTH complete file downloads. */
#if STEREO_FIXED_REPLAY
static volatile u32 stereoReplayCommand[16] __attribute__((aligned(64)));
static u32 replay_hash_step(u32 h, u32 v) { return ((h << 5) | (h >> 27)) ^ v; }
static void replay_input_fingerprint(const u8 *base, u32 *sum, u32 *hash)
{
    u32 x,y,p,g; *sum=0; *hash=0;
    Xil_DCacheInvalidateRange((INTPTR)base, RECTIFY_OUTPUT_BYTES);
    for(y=0;y<720U;y++) for(x=0;x<1280U;x++) {
        p=((const u32 *)base)[y*1920U+x];
        g=(((p>>16)&255U)+((p>>8)&255U)+2U*(p&255U))>>2;
        *sum+=g; *hash=replay_hash_step(*hash,g);
    }
}
static int stereo_fixed_replay(DisplayCtrl *display, XAxiVdma *out_vdma,
                               u8 *left, u8 *right, const u32 *dest)
{
    u32 sum_l,sum_r,hash_l,hash_r,sl,sr,run,us,i,hash;
    XTime begin,now;
    if(StmRead(STEREO_MATCH_BASEADDR,STM_ID)!=STM_CORE_ID ||
       StmRead(STEREO_MATCH_BASEADDR,STM_BUILD)!=0x20260902U) {
        xil_printf("STOP: load diagnostic bitstream first. ID=%08X build=%08X\r\n",
            StmRead(STEREO_MATCH_BASEADDR,STM_ID),StmRead(STEREO_MATCH_BASEADDR,STM_BUILD));
        return XST_FAILURE;
    }
    stereoReplayCommand[0]=0;
    Xil_DCacheFlushRange((INTPTR)stereoReplayCommand,sizeof(stereoReplayCommand));
    xil_printf("\r\n=== FIXED DDR REPLAY: no camera S2MM / no remap runs ===\r\n");
    xil_printf("Copy the following commands to XSCT (Cortex-A9 #0 selected):\r\n");
    xil_printf("stop\r\n");
    xil_printf("dow -data E:/vitis_workspace_dual_camera/capture_left_corrected.bin 0x%08X\r\n",(u32)left);
    xil_printf("dow -data E:/vitis_workspace_dual_camera/capture_right_corrected.bin 0x%08X\r\n",(u32)right);
    xil_printf("mwr 0x%08X 1\r\ncon\r\n",(u32)stereoReplayCommand);
    while(1) {
        Xil_DCacheInvalidateRange((INTPTR)stereoReplayCommand,sizeof(stereoReplayCommand));
        if(stereoReplayCommand[0]==1U) break;
        usleep(10000);
    }
    /* Saved BD: VDMA3/right file -> S_AXIS_LEFT; VDMA2/left -> S_AXIS_RIGHT. */
    replay_input_fingerprint(right,&sum_l,&hash_l);
    replay_input_fingerprint(left,&sum_r,&hash_r);
    xil_printf("CPU expected S_AXIS_LEFT : sum=%08X hash=%08X (right file)\r\n",sum_l,hash_l);
    xil_printf("CPU expected S_AXIS_RIGHT: sum=%08X hash=%08X (left file)\r\n",sum_r,hash_r);
    if(!sum_l || !sum_r) { xil_printf("STOP: empty input\r\n"); return XST_FAILURE; }
    StmWrite(STEREO_MATCH_BASEADDR,STM_DEST0,dest[0]);
    StmWrite(STEREO_MATCH_BASEADDR,STM_DEST1,dest[1]);
    StmWrite(STEREO_MATCH_BASEADDR,STM_DEST2,dest[2]);
    StmWrite(STEREO_MATCH_BASEADDR,STM_WIDTH,DISPARITY_WIDTH);
    StmWrite(STEREO_MATCH_BASEADDR,STM_HEIGHT,DISPARITY_HEIGHT);
    StmWrite(STEREO_MATCH_BASEADDR,STM_STRIDE,DISPARITY_STRIDE);
    StmWrite(STEREO_MATCH_BASEADDR,STM_UNIQUENESS,2U);
    if(disparity_vdma_initialize(out_vdma,dest)!=XST_SUCCESS) return XST_FAILURE;
    /* Every slot points at the SAME immutable image; no old-frame selection. */
    for(i=0;i<DISPLAY_NUM_FRAMES;i++) display_set_corrected_slot(display,i,left,right);
    if(display_park_corrected_pair(display,0)!=XST_SUCCESS) return XST_FAILURE;
    /* PG020: START_ADDRESS writes are pending until VSIZE is written.
     * Commit BOTH channels after all slots and park pointers are programmed.
     * Without this, MM2S keeps reading the initial zero-filled raw buffers.
     */
    for(i=0;i<2U;i++) {
        u32 base=display->vdma[i]->BaseAddr;
        u32 expected=(u32)(i==0U ? left : right);
        u32 slot;
        for(slot=0;slot<DISPLAY_NUM_FRAMES;slot++) {
            u32 actual=XAxiVdma_ReadReg(base,XAXIVDMA_MM2S_ADDR_OFFSET+
                XAXIVDMA_START_ADDR_OFFSET+slot*XAXIVDMA_START_ADDR_LEN);
            if(actual!=expected) {
                xil_printf("STOP: MM2S[%d] slot %d address=%08X expected=%08X\r\n",
                    i,slot,actual,expected);
                return XST_FAILURE;
            }
        }
        XAxiVdma_WriteReg(base,XAXIVDMA_MM2S_ADDR_OFFSET+
            XAXIVDMA_VSIZE_OFFSET,DISPARITY_HEIGHT);
        xil_printf("Replay MM2S[%d] base=%08X all slots=%08X VSIZE committed=%d\r\n",
            i,base,expected,DISPARITY_HEIGHT);
    }
    axis_switch_select_stereo_path();
    sl=StmRead(STEREO_MATCH_BASEADDR,STM_SOF_L);
    sr=StmRead(STEREO_MATCH_BASEADDR,STM_SOF_R);
    XTime_GetTime(&begin);
    /* Drain VDMA/CDC prefetched data while the matcher is unarmed. */
    while((StmRead(STEREO_MATCH_BASEADDR,STM_SOF_L)-sl)<3U ||
          (StmRead(STEREO_MATCH_BASEADDR,STM_SOF_R)-sr)<3U) {
        XTime_GetTime(&now);
        if(remap_elapsed_us(begin,now)>STEREO_TIMEOUT_US) {
            xil_printf("STOP: input SOF drain timeout, check VDMA/switch/CDC\r\n");
            return XST_FAILURE;
        }
        usleep(100);
    }
    for(run=0;run<3;run++) {
        if(stereo_match_run(run,&us)!=XST_SUCCESS) return XST_FAILURE;
        xil_printf("Replay %d time=%d us input=%d sum=%08X,%08X hash=%08X,%08X\r\n",
            run+1,us,StmRead(STEREO_MATCH_BASEADDR,STM_INPUT_COUNT),
            StmRead(STEREO_MATCH_BASEADDR,STM_SUM_L),StmRead(STEREO_MATCH_BASEADDR,STM_SUM_R),
            StmRead(STEREO_MATCH_BASEADDR,STM_HASH_L),StmRead(STEREO_MATCH_BASEADDR,STM_HASH_R));
        if(StmRead(STEREO_MATCH_BASEADDR,STM_INPUT_COUNT)!=1280U*720U ||
           StmRead(STEREO_MATCH_BASEADDR,STM_SUM_L)!=sum_l ||
           StmRead(STEREO_MATCH_BASEADDR,STM_SUM_R)!=sum_r ||
           StmRead(STEREO_MATCH_BASEADDR,STM_HASH_L)!=hash_l ||
           StmRead(STEREO_MATCH_BASEADDR,STM_HASH_R)!=hash_r) {
            xil_printf("STOP: ACTUAL MATCHER INPUT DOES NOT MATCH DDR. Do not tune algorithm.\r\n");
            return XST_FAILURE;
        }
        Xil_DCacheInvalidateRange((INTPTR)dest[run],DISPARITY_FRAME_BYTES);
        hash=0;
        for(i=0;i<DISPARITY_WIDTH*DISPARITY_HEIGHT;i++)
            hash=replay_hash_step(hash,((const u16 *)dest[run])[i]);
        xil_printf("Output DDR hash=%08X core hash=%08X\r\n",hash,
            StmRead(STEREO_MATCH_BASEADDR,STM_OUTPUT_HASH));
        if(hash!=StmRead(STEREO_MATCH_BASEADDR,STM_OUTPUT_HASH)) {
            xil_printf("STOP: CORE OUTPUT / DDR mismatch\r\n"); return XST_FAILURE;
        }
    }
    if(memcmp((void *)dest[0],(void *)dest[1],DISPARITY_FRAME_BYTES) ||
       memcmp((void *)dest[0],(void *)dest[2],DISPARITY_FRAME_BYTES)) {
        xil_printf("STOP: three identical-input runs produced different outputs\r\n");
        return XST_FAILURE;
    }
    if(disparity_vdma_park(out_vdma,0)!=XST_SUCCESS) return XST_FAILURE;
    xil_printf("PASS: exact inputs, core->DDR hash, THREE runs byte-identical.\r\n");
    xil_printf("This is not yet a software-reference accuracy PASS. Export and compare:\r\n");
    xil_printf("mrd -force -size w -bin -file E:/vitis_workspace_dual_camera/verify_left_corrected.bin 0x%08X 0x151800\r\n",(u32)left);
    xil_printf("mrd -force -size w -bin -file E:/vitis_workspace_dual_camera/verify_right_corrected.bin 0x%08X 0x151800\r\n",(u32)right);
    xil_printf("mrd -force -size w -bin -file E:/vitis_workspace_dual_camera/verify_disparity.bin 0x%08X 0x70800\r\n",dest[0]);
    xil_printf("Frozen result ready. HDMI repeats this frame; no need to stop CPU.\r\n");
    return XST_SUCCESS;
}
#endif
