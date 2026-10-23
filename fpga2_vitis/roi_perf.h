#ifndef ROI_PERF_HEADER_INCLUDED
#define ROI_PERF_HEADER_INCLUDED
/* Software-observed launch-to-done times, including DDR/stream stalls.
 * This is the sole reader of ap_done while a timed IP is active. Completion
 * is latched here because the AXI-Lite control register clears done on read.
 * Include after roi_read() and the ROI base-address definitions.
 */
#ifndef ROI_PERF_POLL_US
#define ROI_PERF_POLL_US 10U
#endif
#define ROI_PERF_COUNT 10U
enum { ROI_PERF_A, ROI_PERF_B, ROI_PERF_C, ROI_PERF_D, ROI_PERF_E,
       ROI_PERF_G, ROI_PERF_H, ROI_PERF_I, ROI_PERF_J, ROI_PERF_K };
typedef struct {
 XTime start, end;
 u32 control;
 unsigned started, done;
} roi_perf_module;
static const u32 roi_perf_bases[ROI_PERF_COUNT]={
 ROI_A_GRAY_BASE,ROI_B_TARGET_BASE,ROI_C_CONTOUR_BASE,ROI_D_OVERLAY_BASE,
 ROI_E_REGION_BASE,ROI_G_SUPPLY_BASE,ROI_H_SGM_BASE,ROI_I_MEASURE_BASE,ROI_J_DISPLAY_BASE,ROI_K_DEPTH_BASE
};
static roi_perf_module roi_perf_modules[ROI_PERF_COUNT];
static XTime roi_perf_begin,roi_perf_last_poll,roi_perf_max_gap;
static unsigned roi_perf_poll_valid;

static unsigned roi_perf_index(u32 base)
{
 unsigned i;for(i=0;i<ROI_PERF_COUNT;i++)if(roi_perf_bases[i]==base)return i;
 return ROI_PERF_COUNT;
}
static void roi_perf_reset(void)
{
 unsigned i;
 for(i=0;i<ROI_PERF_COUNT;i++){
  roi_perf_modules[i].start=roi_perf_modules[i].end=0;
  roi_perf_modules[i].control=0;
  roi_perf_modules[i].started=roi_perf_modules[i].done=0;
 }
 roi_perf_poll_valid=0;roi_perf_last_poll=roi_perf_max_gap=0;
 XTime_GetTime(&roi_perf_begin);
}
static void roi_perf_poll(void)
{
 unsigned i,active=0,remaining=0;XTime now;
 for(i=0;i<ROI_PERF_COUNT;i++)if(roi_perf_modules[i].started&&!roi_perf_modules[i].done)active=1;
 if(!active){roi_perf_poll_valid=0;return;}
 XTime_GetTime(&now);
 if(roi_perf_poll_valid&&now-roi_perf_last_poll>roi_perf_max_gap)roi_perf_max_gap=now-roi_perf_last_poll;
 roi_perf_last_poll=now;
 for(i=0;i<ROI_PERF_COUNT;i++){
  roi_perf_module *p=&roi_perf_modules[i];
  if(!p->started||p->done)continue;
  p->control=roi_read(roi_perf_bases[i],0);
  if(p->control&2U){XTime_GetTime(&p->end);p->done=1;}
  else remaining=1;
 }
 roi_perf_poll_valid=remaining;
}
static void roi_perf_mark_start(u32 base)
{
 unsigned i=roi_perf_index(base);
 if(i==ROI_PERF_COUNT)return;
 /* Drop a previous frame's sticky done before taking ownership of this run. */
 (void)roi_read(base,0);
 roi_perf_modules[i].done=0;roi_perf_modules[i].control=0;
 XTime_GetTime(&roi_perf_modules[i].start);
 roi_perf_modules[i].started=1;
}
static int roi_perf_module_us(unsigned i)
{
 const roi_perf_module *p=&roi_perf_modules[i];
 return p->started&&p->done?(int)remap_elapsed_us(p->start,p->end):-1;
}
static void roi_perf_report(u32 frame,u32 width,u32 height,XTime end,u32 present_us)
{
 int chain=-1;
 if(roi_perf_modules[ROI_PERF_G].started&&roi_perf_modules[ROI_PERF_I].done)
  chain=(int)remap_elapsed_us(roi_perf_modules[ROI_PERF_G].start,roi_perf_modules[ROI_PERF_I].end);
 xil_printf("PERF frame=%d roi=%dx%d units=us observed=SW poll=%d gap_max=%d\r\n",
  frame,width,height,ROI_PERF_POLL_US,remap_elapsed_us(0,roi_perf_max_gap));
 xil_printf("PERF IP A=%d B=%d C=%d D=%d E=%d G=%d H=%d I=%d J=%d\r\n",
  roi_perf_module_us(ROI_PERF_A),roi_perf_module_us(ROI_PERF_B),roi_perf_module_us(ROI_PERF_C),
  roi_perf_module_us(ROI_PERF_D),roi_perf_module_us(ROI_PERF_E),roi_perf_module_us(ROI_PERF_G),
  roi_perf_module_us(ROI_PERF_H),roi_perf_module_us(ROI_PERF_I),roi_perf_module_us(ROI_PERF_J));
 xil_printf("PERF GHI=%d total=%d prep=%d present=%d (-1=not_run_or_incomplete)\r\n",chain,
  remap_elapsed_us(roi_perf_begin,end),remap_elapsed_us(roi_perf_begin,roi_perf_modules[ROI_PERF_A].start),present_us);
}
#endif
