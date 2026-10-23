#ifndef ROI_DUAL_MODE_H
#define ROI_DUAL_MODE_H
#include "xroi_k_depth_hw.h"

static unsigned roi_ui_initialized,roi_display_mode;
static u32 roi_frame_counter;
static void roi_alarm_off(void) {
 if(roi_ui_initialized)Xil_Out32(ROI_UI_GPIO_BASE+8U,0U);
}
static void roi_alarm_update(u32 frame,const u32 *r) {
 int valid=r[0]==0x44505431U && r[1]==1U && !r[2] && r[3]==frame && r[4]==frame &&
  r[15]==1U && r[32]==0x58595A31U && r[33]==1U && r[35]==frame &&
  !r[36] && r[37]==1U && r[59]==1U && r[60]==1000U && !r[64] && r[65]==frame;
 int32_t z=(int32_t)r[50];
 unsigned alarm=valid && !roi_display_mode && z>0 && z<=500000;
 Xil_Out32(ROI_UI_GPIO_BASE+8U,alarm);
}
static void roi_palette_init(void) {
 u16 *lut=(u16 *)(UINTPTR)ROI_DEPTH_PALETTE;unsigned d;
 for(d=0;d<256;d++) {
  unsigned red=0,green=0,blue=0;
  if(d>=1 && d<=254 && (float)d>OVERLAY_PRINCIPAL_DELTA) {
   float z=OVERLAY_NUMERATOR/((float)d-OVERLAY_PRINCIPAL_DELTA);
   float t=(z-200.0f)*(4.0f/1800.0f);
   unsigned q;
   if(t<0)t=0;if(t>4)t=4;
   q=(unsigned)(t*255.0f);
   if(q<255){red=255;green=q;}
   else if(q<510){red=510-q;green=255;}
   else if(q<765){green=255;blue=q-510;}
   else {green=1020-q;blue=255;}
  }
  lut[d]=(u16)(((red>>3)<<11)|((green>>2)<<5)|(blue>>3));
 }
 Xil_DCacheFlushRange(ROI_DEPTH_PALETTE,512);
}
static void roi_ui_init(void) {
 Xil_Out32(ROI_UI_GPIO_BASE+4U,3U);  /* channel 1: debounced mode input */
 Xil_Out32(ROI_UI_GPIO_BASE+8U,0U);  /* channel 2: active-high alarm */
 Xil_Out32(ROI_UI_GPIO_BASE+12U,0U);
 roi_ui_initialized=1;roi_display_mode=0;roi_palette_init();
 xil_printf("TRIPLE MODE: PL_KEY0/SW4 cycles RANGE/DEPTH/FUSION; PL_LED0/LED4 on for valid Z<=500mm.\r\n");
}
static void roi_k_config(unsigned mode,unsigned rows,unsigned cols,u32 rgb) {
 roi_ptr(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_RGB565_DATA,rgb);
 roi_ptr(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_PALETTE_DATA,ROI_DEPTH_PALETTE);
 roi_write(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_MODE_DATA,mode);
 roi_write(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_ROWS_DATA,rows);
 roi_write(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_COLS_DATA,cols);
}
static int roi_depth_once(const u8 *left,const u8 *right) {
 u32 frame=++roi_frame_counter;
 unsigned slot=stream_display_started?(stream_front^1U):0U;
 u32 rgb=slot?STREAM_RGB1:STREAM_RGB0;
 XTime end,present_begin,present_end;
 roi_alarm_off();roi_perf_reset();
 Xil_DCacheFlushRange((INTPTR)left,RECTIFY_OUTPUT_BYTES);
 Xil_DCacheFlushRange((INTPTR)right,RECTIFY_OUTPUT_BYTES);
 roi_pl_output(ROI_GRAY_LEFT,ROI_WIDTH*ROI_HEIGHT);
 roi_pl_output(ROI_GRAY_RIGHT,ROI_WIDTH*ROI_HEIGHT);
 roi_pl_output(rgb,STREAM_RGB_BYTES);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_LEFT_R_DATA,(u32)(UINTPTR)left);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_RIGHT_R_DATA,(u32)(UINTPTR)right);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_LEFT_GRAY_DATA,ROI_GRAY_LEFT);
 roi_ptr(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_RIGHT_GRAY_DATA,ROI_GRAY_RIGHT);
 roi_write(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_write(ROI_A_GRAY_BASE,XROI_A_GRAY_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA,ROI_COLOR_STRIDE_WORDS);
 roi_start(ROI_A_GRAY_BASE);
 if(roi_wait(ROI_A_GRAY_BASE,"A full gray"))return XST_FAILURE;
 roi_k_config(roi_display_mode,ROI_HEIGHT,ROI_WIDTH,rgb);
 roi_ptr(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_IMAGE_R_DATA,(u32)(UINTPTR)right);
 roi_write(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA,ROI_COLOR_STRIDE_WORDS);
 roi_write(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_BLEND_EIGHTHS_DATA,ROI_FUSION_EIGHTHS);
 roi_write(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_ROWS_DATA,ROI_HEIGHT);
 roi_write(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_COLS_DATA,ROI_WIDTH);
 roi_ptr(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_LEFT_GRAY_DATA,ROI_GRAY_LEFT);
 roi_ptr(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_RIGHT_GRAY_DATA,ROI_GRAY_RIGHT);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_X0_DATA,0);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_Y0_DATA,0);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_WIDTH_DATA,ROI_WIDTH);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_HEIGHT_DATA,ROI_HEIGHT);
 roi_write(ROI_G_SUPPLY_BASE,XROI_G_SUPPLY_CONTROL_ADDR_STRIDE_DATA,ROI_WIDTH);
 /* Always start consumers first. I stays idle in this mode. */
 roi_start(ROI_K_DEPTH_BASE);roi_start(ROI_H_SGM_BASE);roi_start(ROI_G_SUPPLY_BASE);
 if(roi_wait(ROI_G_SUPPLY_BASE,"G full supply")||roi_wait(ROI_H_SGM_BASE,"H full SGM")||
    roi_wait(ROI_K_DEPTH_BASE,"K full depth"))return XST_FAILURE;
 if(roi_read(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_REFERENCE_ERRORS_DATA)||
    roi_read(ROI_H_SGM_BASE,XROI_H_SGM_CONTROL_ADDR_SEARCH_ERRORS_DATA)||
    roi_read(ROI_K_DEPTH_BASE,XROI_K_DEPTH_CONTROL_ADDR_ERRORS_DATA))return XST_FAILURE;
 XTime_GetTime(&present_begin);
 if(stream_present_rgb(slot))return XST_FAILURE;
 XTime_GetTime(&present_end);XTime_GetTime(&end);
 overlay_request[0]=roi_display_mode==2U?0x4655534EU:0x44505448U;overlay_request[1]=frame;overlay_request[7]=rgb;overlay_request[14]=0;
 Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
 xil_printf("PERF %s frame=%d full=1280x720 A=%d G=%d H=%d K=%d total=%d present=%d units=us rgb=%08X\r\n",
  roi_display_mode==2U?"FUSION":"DEPTH",frame,roi_perf_module_us(ROI_PERF_A),roi_perf_module_us(ROI_PERF_G),roi_perf_module_us(ROI_PERF_H),
  roi_perf_module_us(ROI_PERF_K),remap_elapsed_us(roi_perf_begin,end),remap_elapsed_us(present_begin,present_end),rgb);
 return XST_SUCCESS;
}
#endif
