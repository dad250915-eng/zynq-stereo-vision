/* Board adapter; included once by frozen_overlay_capture.h. */
#include "xyz_diagnostic.h"
#include <stdio.h>
volatile u32 xyz_debug[XYZ_WORDS] __attribute__((aligned(64)));

static void xyz_commit(u32 r[XYZ_WORDS])
{
    unsigned i;XTime stamp;u32 seq=(xyz_debug[2]+1U)|1U;
    XTime_GetTime(&stamp);r[24]=(u32)stamp;r[25]=(u32)(stamp>>32);
    /* Publish odd sequence to DDR before overwriting any payload. */
    xyz_debug[2]=seq;
    Xil_DCacheFlushRange((INTPTR)xyz_debug,sizeof(xyz_debug));
    for(i=0;i<XYZ_WORDS;i++)if(i!=2)xyz_debug[i]=r[i];
    Xil_DCacheFlushRange((INTPTR)xyz_debug,sizeof(xyz_debug));
    xyz_debug[2]=seq+1U;
    Xil_DCacheFlushRange((INTPTR)xyz_debug,sizeof(xyz_debug));
}
static void xyz_state(u32 frame,u32 status)
{
    u32 r[XYZ_WORDS]={0};
    r[0]=XYZ_MAGIC;r[1]=1;r[3]=frame;r[4]=status;
    r[8]=XYZ_RADIUS;r[27]=1;r[28]=1000;
    r[29]=XYZ_MIN_SUPPORT;r[30]=XYZ_MAX_IQR_PX*16;
    xyz_commit(r);
}
static void xyz_print_mm(int32_t v)
{
    /* Integers avoid xil_printf's unsupported floating-point formats. */
    unsigned a=v<0?(unsigned)(-v):(unsigned)v;
    xil_printf("%s%d.%03d",v<0?"-":"",a/1000,a%1000);
}
static void xyz_diagnostic_publish(u32 frame)
{
    u32 r[XYZ_WORDS],addr=overlay_request[6];
    XTime begin,end;unsigned x,y,u,v;
    if(overlay_request[1]!=frame||
       (addr!=0x07000000U&&addr!=0x07200000U&&addr!=0x07400000U)){
        xyz_state(frame,XYZ_BAD_METADATA);return;
    }
    XTime_GetTime(&begin);
    Xil_DCacheInvalidateRange((INTPTR)addr,1280U*720U*2U);
    xyz_compute((const uint16_t *)(UINTPTR)addr,2560,overlay_mask,1280,
                overlay_depth_result,1280,720,frame,r);
    XTime_GetTime(&end);r[26]=remap_elapsed_us(begin,end);
    xyz_commit(r);
    if(r[5]){
        xil_printf("XYZ RIGHT_RECT frame=%08X valid=1 uv=(%d,%d) d_q4=%d X_mm=",frame,r[6],r[7],r[12]);
        xyz_print_mm((int32_t)r[16]);xil_printf(" Y_mm=");xyz_print_mm((int32_t)r[17]);
        xil_printf(" Z_mm=");xyz_print_mm((int32_t)r[18]);
        xil_printf(" n=%d/%d support=%d/1000 iqr_q4=%d calc_us=%d\r\n",r[10],r[9],r[15],r[14],r[26]);
        /* Draw only on the CPU-owned BGRX output, before RGB565 presentation.
         * Preserve all source images/mask/disparity. Active image starts x=320. */
        u=r[6];v=r[7];
        for(y=v-XYZ_RADIUS;y<=v+XYZ_RADIUS;y++)
            for(x=u-XYZ_RADIUS;x<=u+XYZ_RADIUS;x++)
                if(x==u-XYZ_RADIUS||x==u+XYZ_RADIUS||y==v-XYZ_RADIUS||y==v+XYZ_RADIUS||
                   (x==u&&y+2>=v&&y<=v+2)||(y==v&&x+2>=u&&x<=u+2)){
                    u32 color=0x0000FF00U;
                    memcpy(overlay_display+y*7680+(x+320)*4,&color,4);
                }
    }else{
        xil_printf("XYZ RIGHT_RECT frame=%08X valid=0 reason=%s global_status=%d interior=%d passing=%d calc_us=%d\r\n",
                   frame,xyz_status_name(r[4]),r[19],r[22],r[23],r[26]);
    }
}
