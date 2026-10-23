#ifndef OVERLAY_IP_CONFIG_H
#define OVERLAY_IP_CONFIG_H
/* HSV H=0..179, S/V=0..255. Pale yellow/cream bench-test range (capture 20260922_211343),
 * Scene-specific: H=0..45, S=0..100, V=120..255; verify on board. Packing: 0x00VVSSHH. */
#define OVERLAY_HSV_LOW  0x00780000U
#define OVERLAY_HSV_HIGH 0x00FF642DU
#define OVERLAY_MIN_AREA 200U
#define OVERLAY_MAX_AREA 307200U
#define OVERLAY_MIN_POINTS 100U
#define OVERLAY_MIN_SUPPORT 500U
/* Experimental display tolerance; relaxed from 16. Not a depth accuracy guarantee. */
#define OVERLAY_MAX_IQR 48U
/* Current calibration/right-reference convention. mm*pixel and pixels. */
#define OVERLAY_NUMERATOR 44268.252199f
#define OVERLAY_PRINCIPAL_DELTA 0.0f
#define OVERLAY_TIMEOUT_US 10000000U
#endif
