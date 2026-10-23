/* Included after the existing rectification and AXIS display helpers. */
#include "roi_pipeline_config.h"
#ifndef ROI_XYZ_UART_H
#define ROI_XYZ_UART_H
#include <stdint.h>

/* I produces the legacy 32-word XYZ record at result[32]. Coordinates remain
 * RIGHT_RECT, in signed 0.001 mm units. Publish after PL completion/cache
 * invalidation; this adapter never recomputes or transforms the measurement.
 * calc_us is software-observed I launch-to-done time, including stream stalls.
 * xil_printf must be declared by the caller's BSP (or the host test harness). */
static void roi_xyz_print_mm(int32_t value)
{
    uint32_t magnitude = value < 0 ? 0U-(uint32_t)value : (uint32_t)value;
    xil_printf("%s%d.%03d", value < 0 ? "-" : "", magnitude/1000U, magnitude%1000U);
}

static const char *roi_xyz_status_name(uint32_t status)
{
    switch(status) {
    case 0: return "OK";
    case 1: return "BAD_METADATA";
    case 2: return "WRONG_FRAME";
    case 3: return "GLOBAL_INVALID";
    case 4: return "MASK_MISMATCH";
    case 5: return "NO_INTERIOR";
    case 6: return "LOW_SUPPORT";
    case 7: return "SPREAD";
    case 8: return "BAD_GEOMETRY";
    case 9: return "PIPELINE_STOP";
    case 10: return "BUSY";
    default: return "UNKNOWN_STATUS";
    }
}

static void roi_xyz_publish(uint32_t frame, const uint32_t result[66], uint32_t calc_us)
{
    const uint32_t *xyz = result+32;
    int metadata = result[0]==0x44505431U && result[1]==1U &&
        result[3]==frame && result[4]==frame && !result[64] && result[65]==frame &&
        xyz[0]==0x58595A31U && xyz[1]==1U && xyz[3]==frame &&
        xyz[27]==1U && xyz[28]==1000U;
    if(metadata && !result[2] && result[15]==1U && !xyz[4] && xyz[5]==1U) {
        xil_printf("XYZ RIGHT_RECT frame=%08X valid=1 uv=(%d,%d) d_q4=%d X_mm=",
            frame,xyz[6],xyz[7],xyz[12]);
        roi_xyz_print_mm((int32_t)xyz[16]);
        xil_printf(" Y_mm="); roi_xyz_print_mm((int32_t)xyz[17]);
        xil_printf(" Z_mm="); roi_xyz_print_mm((int32_t)xyz[18]);
        xil_printf(" n=%d/%d support=%d/1000 iqr_q4=%d calc_us=%d\r\n",
            xyz[10],xyz[9],xyz[15],xyz[14],calc_us);
    } else {
        const char *reason = !metadata ? "BAD_METADATA" :
            (xyz[4] ? roi_xyz_status_name(xyz[4]) : "INVALID_RESULT");
        xil_printf("XYZ RIGHT_RECT frame=%08X valid=0 reason=%s global_status=%d interior=%d passing=%d calc_us=%d\r\n",
            frame,reason,result[2],xyz[22],xyz[23],calc_us);
    }
}
#endif

#include "overlay_ip_config.h"
volatile u32 overlay_request[16] __attribute__((aligned(64)));
#include "stream_display.h"
#include "xroi_a_gray_hw.h"
#include "xroi_b_target_hw.h"
#include "xroi_c_contour_hw.h"
#include "xroi_d_overlay_hw.h"
#include "xroi_e_region_hw.h"
#include "xroi_g_supply_hw.h"
#include "xroi_h_sgm_hw.h"
#include "xroi_i_measure_hw.h"
#include "xroi_j_display_hw.h"

static u32 roi_read(u32 base,u32 offset){return Xil_In32(base+offset);}
#include "roi_perf.h"
static void roi_write(u32 base,u32 offset,u32 value){Xil_Out32(base+offset,value);}
static void roi_ptr(u32 base,u32 offset,u32 value){roi_write(base,offset,value);roi_write(base,offset+4,0);}
static u32 roi_float(float value){u32 bits;memcpy(&bits,&value,4);return bits;}
static int roi_wait(u32 base,const char *name)
{
 unsigned i=roi_perf_index(base);XTime begin,now;XTime_GetTime(&begin);
 if(i==ROI_PERF_COUNT||!roi_perf_modules[i].started)return XST_FAILURE;
 while(!roi_perf_modules[i].done){
  roi_perf_poll();
  if(roi_perf_modules[i].done)break;
  XTime_GetTime(&now);
  if(remap_elapsed_us(begin,now)>ROI_TIMEOUT_US){xil_printf("ROI timeout: %s ctrl=%08X; reload PL before retry.\r\n",name,roi_perf_modules[i].control);return XST_FAILURE;}
  if(ROI_PERF_POLL_US)usleep(ROI_PERF_POLL_US);
 }
 return XST_SUCCESS;
}
static void roi_start(u32 base){roi_perf_poll();roi_perf_mark_start(base);roi_write(base,0,1);roi_perf_poll();}
static void roi_pl_output(u32 addr,u32 bytes)
{ /* Remove dirty CPU lines before giving ownership to a PL writer. */
 Xil_DCacheFlushRange(addr,bytes);Xil_DCacheInvalidateRange(addr,bytes);
}
#include "roi_dual_mode.h"
static int roi_preflight(void)
{
 const u32 bases[10]={ROI_A_GRAY_BASE,ROI_B_TARGET_BASE,ROI_C_CONTOUR_BASE,ROI_D_OVERLAY_BASE,ROI_E_REGION_BASE,ROI_G_SUPPLY_BASE,ROI_H_SGM_BASE,ROI_I_MEASURE_BASE,ROI_J_DISPLAY_BASE,ROI_K_DEPTH_BASE};
 unsigned i;XAxiVdma_Config *v=XAxiVdma_LookupConfig(DISPARITY_VDMA_DEVICE_ID);
 if(!v||!(XAxiVdma_ReadReg(v->BaseAddress,XAXIVDMA_SR_OFFSET)&XAXIVDMA_SR_HALTED_MASK))return XST_FAILURE;
 for(i=0;i<10;i++)if(!(roi_read(bases[i],0)&4U)){xil_printf("ROI IP busy at %08X; reload PL before starting.\r\n",bases[i]);return XST_FAILURE;}
 roi_ui_init();
 /* HDMI selects the completed RGB565 frame from VDMA4. */
 Xil_Out32(DISPLAY_SWITCH_BASEADDR+AXIS_SWITCH_MI_MUX_BASE,1U);
 Xil_Out32(DISPLAY_SWITCH_BASEADDR+AXIS_SWITCH_CTRL,AXIS_SWITCH_REG_UPDATE);
 xil_printf("ROI architecture: RX1=LEFT, RX0=RIGHT, reference=RIGHT_RECT, d=uR-uL.\r\n");
 return XST_SUCCESS;
}
static int roi_capture_once(const u8 *left,const u8 *right)
{
 u32 frame=++roi_frame_counter;u32 *m=(u32 *)(UINTPTR)ROI_META,*d=(u32 *)(UINTPTR)ROI_DESC,*r=(u32 *)(UINTPTR)ROI_RESULT;
 unsigned slot=stream_display_started?(stream_front^1U):0U;u32 rgb=slot?STREAM_RGB1:STREAM_RGB0;unsigned k;
 XTime present_begin,present_end,perf_end;
 roi_perf_reset();
 Xil_DCacheFlushRange((INTPTR)left,RECTIFY_OUTPUT_BYTES);
 Xil_DCacheFlushRange((INTPTR)right,RECTIFY_OUTPUT_BYTES);
 roi_pl_output(ROI_GRAY_LEFT,ROI_WIDTH*ROI_HEIGHT);roi_pl_output(ROI_GRAY_RIGHT,ROI_WIDTH*ROI_HEIGHT);
 roi_pl_output(ROI_WORK_MASK,ROI_WIDTH*ROI_HEIGHT);roi_pl_output(ROI_QUEUE,ROI_WIDTH*ROI_HEIGHT*4U);
 roi_pl_output(ROI_META,64);roi_pl_output(ROI_DESC,64);roi_pl_output(ROI_RESULT,320);
 roi_pl_output(ROI_MASK,ROI_WIDTH*ROI_HEIGHT);roi_pl_output(ROI_CONTOUR,ROI_WIDTH*ROI_HEIGHT);
 roi_pl_output(ROI_BASE_IMAGE,RECTIFY_OUTPUT_BYTES);roi_pl_output(rgb,STREAM_RGB_BYTES);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_LEFT_R_DATA,(u32)(UINTPTR)left);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_RIGHT_R_DATA,(u32)(UINTPTR)right);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_LEFT_GRAY_DATA,ROI_GRAY_LEFT);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_RIGHT_GRAY_DATA,ROI_GRAY_RIGHT);
 roi_write(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_write(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA,ROI_COLOR_STRIDE_WORDS);
 roi_ptr(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_IMAGE_R_DATA,(u32)(UINTPTR)right);
 roi_ptr(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_WORK_MASK_DATA,ROI_WORK_MASK);
 roi_ptr(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_QUEUE_DATA,ROI_QUEUE);
 roi_ptr(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_META_DATA,ROI_META);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA,ROI_COLOR_STRIDE_WORDS);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_LOW_R_DATA,OVERLAY_HSV_LOW);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_HIGH_R_DATA,OVERLAY_HSV_HIGH);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_MIN_AREA_DATA,OVERLAY_MIN_AREA);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_MAX_AREA_DATA,OVERLAY_MAX_AREA);
 roi_write(ROI_B_TARGET_BASE,XROI_B_TARGET_CONTROL_ADDR_FRAME_DATA,frame);
 roi_start(ROI_A_GRAY_BASE);roi_start(ROI_B_TARGET_BASE);
 if(roi_wait(ROI_B_TARGET_BASE,"B target"))return XST_FAILURE;
 Xil_DCacheInvalidateRange(ROI_META,64);
 if(m[0]!=0x54475431U||m[1]!=frame||(m[2]&~4U)){xil_printf("ROI invalid target metadata status=%08X\r\n",m[2]);return XST_FAILURE;}
 roi_ptr(ROI_C_CONTOUR_BASE,XROI_C_CONTOUR_CONTROL_ADDR_MASK_DATA,ROI_WORK_MASK);
 roi_ptr(ROI_C_CONTOUR_BASE,XROI_C_CONTOUR_CONTROL_ADDR_CONTOUR_DATA,ROI_CONTOUR);
 roi_write(ROI_C_CONTOUR_BASE,XROI_C_CONTOUR_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_C_CONTOUR_BASE,XROI_C_CONTOUR_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_ptr(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_IMAGE_R_DATA,(u32)(UINTPTR)right);
 roi_ptr(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_SELECTED_MASK_DATA,ROI_WORK_MASK);
 roi_ptr(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_META_DATA,ROI_META);
 roi_ptr(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_MASK_DATA,ROI_MASK);
 roi_ptr(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_BASE_IMAGE_DATA,ROI_BASE_IMAGE);
 roi_write(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_write(ROI_D_OVERLAY_BASE,XROI_D_OVERLAY_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA,ROI_COLOR_STRIDE_WORDS);
 roi_ptr(ROI_E_REGION_BASE,XROI_E_REGION_CONTROL_ADDR_META_DATA,ROI_META);
 roi_ptr(ROI_E_REGION_BASE,XROI_E_REGION_CONTROL_ADDR_DESCRIPTOR_DATA,ROI_DESC);
 roi_write(ROI_E_REGION_BASE,XROI_E_REGION_CONTROL_ADDR_MAX_DISPARITY_DATA,ROI_SEARCH_MAX);
 roi_write(ROI_E_REGION_BASE,XROI_E_REGION_CONTROL_ADDR_CONTEXT_R_DATA,ROI_CONTEXT);
 roi_start(ROI_C_CONTOUR_BASE);roi_start(ROI_D_OVERLAY_BASE);roi_start(ROI_E_REGION_BASE);
 if(roi_wait(ROI_A_GRAY_BASE,"A gray")||roi_wait(ROI_E_REGION_BASE,"E region"))return XST_FAILURE;
 Xil_DCacheInvalidateRange(ROI_DESC,64);
 if(d[0]!=0x524F4931U||d[1]!=frame||d[9]!=frame)return XST_FAILURE;
 if(!d[2]){
 /* B has completed its full 0/255 mask. C, D and I only read it until
  * this function returns; the next B frame cannot overwrite it early. */
 roi_ptr(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MASK_DATA,ROI_WORK_MASK);
 roi_ptr(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_META_DATA,ROI_META);
 roi_ptr(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_RESULT_DATA,ROI_RESULT);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_X0_DATA,d[3]);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_Y0_DATA,d[4]);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_WIDTH_DATA,d[5]);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_HEIGHT_DATA,d[6]);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_FRAME_DATA,frame);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MASK_FRAME_DATA,frame);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MIN_DISPARITY_DATA,1U);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MAX_DISPARITY_DATA,254U);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MIN_POINTS_DATA,OVERLAY_MIN_POINTS);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MIN_SUPPORT_DATA,OVERLAY_MIN_SUPPORT);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_MAX_IQR_DATA,OVERLAY_MAX_IQR);
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_NUMERATOR_DATA,roi_float(OVERLAY_NUMERATOR));
 roi_write(ROI_I_MEASURE_BASE,XROI_I_MEASURE_CONTROL_ADDR_DELTA_DATA,roi_float(OVERLAY_PRINCIPAL_DELTA));
 roi_write(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_ROWS_DATA,d[6]);
 roi_write(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_COLS_DATA,d[5]);
 roi_ptr(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_LEFT_GRAY_DATA,ROI_GRAY_LEFT);
 roi_ptr(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_RIGHT_GRAY_DATA,ROI_GRAY_RIGHT);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_X0_DATA,d[3]);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_Y0_DATA,d[4]);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_WIDTH_DATA,d[5]);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_HEIGHT_DATA,d[6]);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_STRIDE_DATA,ROI_WIDTH);
 roi_k_config(0,d[6],d[5],rgb);
 roi_start(ROI_I_MEASURE_BASE);roi_start(ROI_K_DEPTH_BASE);roi_start(ROI_H_SGM_BASE);roi_start(ROI_G_SUPPLY_BASE);
 if(roi_wait(ROI_G_SUPPLY_BASE,"G supply")||roi_wait(ROI_H_SGM_BASE,"H census/SGM")||roi_wait(ROI_I_MEASURE_BASE,"I XYZ")||roi_wait(ROI_K_DEPTH_BASE,"K measurement pass"))return XST_FAILURE;
 Xil_DCacheInvalidateRange(ROI_RESULT,320);
 if(roi_read(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_REFERENCE_ERRORS_DATA)||roi_read(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_SEARCH_ERRORS_DATA)||roi_read(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_ERRORS_DATA)||r[64]||r[65]!=frame){xil_printf("ROI stream/frame protocol failure\r\n");return XST_FAILURE;}
 }else{
  /* Empty target: publish a fresh invalid result instead of the previous Z. */
  if(!(m[2]&4U))return XST_FAILURE;
  for(k=0;k<66;k++)r[k]=0;
  r[0]=0x44505431U;r[1]=1;r[2]=12;r[3]=frame;r[4]=frame;r[20]=ROI_HEIGHT;r[21]=ROI_WIDTH;
  r[32]=0x58595A31U;r[33]=1;r[35]=frame;r[36]=3;r[59]=1;r[60]=1000;r[65]=frame;
  Xil_DCacheFlushRange(ROI_RESULT,320);
 }
 roi_alarm_update(frame,r);
 /* Join both display producers after the independent G/H/I pipeline.
  * roi_wait polls/latches all active IPs, so D timing is still observed
  * while waiting for G/H/I; reading ap_done here cannot lose completion. */
 if(roi_wait(ROI_C_CONTOUR_BASE,"C contour")||roi_wait(ROI_D_OVERLAY_BASE,"D red box/mask"))return XST_FAILURE;
 roi_ptr(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_BASE_IMAGE_DATA,ROI_BASE_IMAGE);
 roi_ptr(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_CONTOUR_DATA,ROI_CONTOUR);
 roi_ptr(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_RESULT_DATA,ROI_RESULT);
 roi_ptr(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_RGB565_DATA,rgb);
 roi_write(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_write(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA,ROI_COLOR_STRIDE_WORDS);
 roi_write(ROI_J_DISPLAY_BASE,XROI_J_DISPLAY_CONTROL_ADDR_FRAME_DATA,frame);
 roi_start(ROI_J_DISPLAY_BASE);if(roi_wait(ROI_J_DISPLAY_BASE,"J display"))return XST_FAILURE;
 /* PL output is complete; stream_present_rgb parks only on a finished frame. */
 XTime_GetTime(&present_begin);
 if(stream_present_rgb(slot))return XST_FAILURE;
 XTime_GetTime(&present_end);
 overlay_request[0]=0x524F4931U;overlay_request[1]=frame;overlay_request[2]=(u32)(UINTPTR)left;overlay_request[3]=(u32)(UINTPTR)right;
 overlay_request[4]=ROI_META;overlay_request[5]=ROI_DESC;overlay_request[6]=ROI_RESULT;overlay_request[7]=rgb;overlay_request[14]=0;
 Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
 XTime_GetTime(&perf_end);
 roi_perf_report(frame,d[5],d[6],perf_end,remap_elapsed_us(present_begin,present_end));
 { const roi_perf_module *pd=&roi_perf_modules[ROI_PERF_D];
   const roi_perf_module *pg=&roi_perf_modules[ROI_PERF_G];
   const roi_perf_module *pi=&roi_perf_modules[ROI_PERF_I];
   int overlap=-1;
   if(pd->done&&pg->started&&pi->done){
    XTime start=pd->start>pg->start?pd->start:pg->start;
    XTime end=pd->end<pi->end?pd->end:pi->end;
    overlap=end>start?(int)remap_elapsed_us(start,end):0;
   }
   xil_printf("PERF overlap_D_GHI=%d units=us mask_source=B (-1=GHI_not_run)\r\n",overlap);
 }
 xil_printf("ROI frame=%d rect=(%d,%d,%d,%d) area=%d depth_status=%d XYZ_status=%d rgb=%08X\r\n",frame,d[3],d[4],d[5],d[6],m[3],r[2],r[36],rgb);
 /* Every successful round publishes valid=1 or valid=0; never replay old XYZ. */
 { int observed=roi_perf_module_us(ROI_PERF_I);
   roi_xyz_publish(frame,r,observed<0?0U:(u32)observed); }
 return XST_SUCCESS;
}

/* Both paths have joined every active producer before returning here. Only
 * sample the latched button mode at a complete frame boundary. */
static int roi_dual_capture_once(const u8 *left,const u8 *right) {
 unsigned requested=Xil_In32(ROI_UI_GPIO_BASE)&3U;
 int status;
 if(requested>2U){roi_alarm_off();return XST_FAILURE;}
 if(requested!=roi_display_mode) {
  roi_alarm_off();roi_display_mode=requested;
  xil_printf("MODE %s\r\n",requested==2U?"FUSION_FULL":(requested?"DEPTH_FULL":"RANGE_ROI"));
  if(requested)xil_printf("XYZ RIGHT_RECT frame=%08X valid=0 reason=DISPLAY_MODE global_status=0 interior=0 passing=0 calc_us=0\r\n",roi_frame_counter+1U);
 }
 status=roi_display_mode?roi_depth_once(left,right):roi_capture_once(left,right);
 if(status!=XST_SUCCESS)roi_alarm_off();
 return status;
}
