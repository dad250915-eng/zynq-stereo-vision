#ifndef SGM2_CONFIG_H
#define SGM2_CONFIG_H
/* New architecture is the default; 0 selects archived legacy code only. */
#ifndef ROI_ARCHITECTURE
#define ROI_ARCHITECTURE 1
#endif
/* Opt-in: requires VDMA4 RGB565 display BD and its new bitstream. */
#ifndef SGM2_CONTINUOUS
#define SGM2_CONTINUOUS ROI_ARCHITECTURE
#endif
/* New SGM2 hardware is the default; set 0 only when loading the old STM1 bitstream. */
#ifndef SGM2_ENABLED
#define SGM2_ENABLED 1
#endif
/* 0: first live corrected pair, then freeze. 1: debugger-loaded fixed pair. */
#ifndef SGM2_FIXED_REPLAY
#define SGM2_FIXED_REPLAY 0
#endif
/* 1: frozen right color overlay; hardware color/mask/depth pipeline. */
#ifndef OVERLAY_DEBUG_HANDOFF
#define OVERLAY_DEBUG_HANDOFF 0
#endif
#ifndef SGM2_FROZEN_OVERLAY
#define SGM2_FROZEN_OVERLAY 1
#endif
#endif
