// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2023.2 (64-bit)
// Tool Version Limit: 2023.10
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// control
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read/COR)
//        bit 7  - auto_restart (Read/Write)
//        bit 9  - interrupt (Read)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0 - enable ap_done interrupt (Read/Write)
//        bit 1 - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0 - ap_done (Read/TOW)
//        bit 1 - ap_ready (Read/TOW)
//        others - reserved
// 0x10 : Data signal of rows
//        bit 31~0 - rows[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of cols
//        bit 31~0 - cols[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of hsv_low
//        bit 31~0 - hsv_low[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of hsv_high
//        bit 31~0 - hsv_high[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of frame_id
//        bit 31~0 - frame_id[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of result_frame_id
//        bit 31~0 - result_frame_id[31:0] (Read)
// 0x3c : Control signal of result_frame_id
//        bit 0  - result_frame_id_ap_vld (Read/COR)
//        others - reserved
// 0x48 : Data signal of status
//        bit 31~0 - status[31:0] (Read)
// 0x4c : Control signal of status
//        bit 0  - status_ap_vld (Read/COR)
//        others - reserved
// 0x58 : Data signal of output_pixels
//        bit 31~0 - output_pixels[31:0] (Read)
// 0x5c : Control signal of output_pixels
//        bit 0  - output_pixels_ap_vld (Read/COR)
//        others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCOLOR_DIVIDER_CONTROL_ADDR_AP_CTRL              0x00
#define XCOLOR_DIVIDER_CONTROL_ADDR_GIE                  0x04
#define XCOLOR_DIVIDER_CONTROL_ADDR_IER                  0x08
#define XCOLOR_DIVIDER_CONTROL_ADDR_ISR                  0x0c
#define XCOLOR_DIVIDER_CONTROL_ADDR_ROWS_DATA            0x10
#define XCOLOR_DIVIDER_CONTROL_BITS_ROWS_DATA            32
#define XCOLOR_DIVIDER_CONTROL_ADDR_COLS_DATA            0x18
#define XCOLOR_DIVIDER_CONTROL_BITS_COLS_DATA            32
#define XCOLOR_DIVIDER_CONTROL_ADDR_HSV_LOW_DATA         0x20
#define XCOLOR_DIVIDER_CONTROL_BITS_HSV_LOW_DATA         32
#define XCOLOR_DIVIDER_CONTROL_ADDR_HSV_HIGH_DATA        0x28
#define XCOLOR_DIVIDER_CONTROL_BITS_HSV_HIGH_DATA        32
#define XCOLOR_DIVIDER_CONTROL_ADDR_FRAME_ID_DATA        0x30
#define XCOLOR_DIVIDER_CONTROL_BITS_FRAME_ID_DATA        32
#define XCOLOR_DIVIDER_CONTROL_ADDR_RESULT_FRAME_ID_DATA 0x38
#define XCOLOR_DIVIDER_CONTROL_BITS_RESULT_FRAME_ID_DATA 32
#define XCOLOR_DIVIDER_CONTROL_ADDR_RESULT_FRAME_ID_CTRL 0x3c
#define XCOLOR_DIVIDER_CONTROL_ADDR_STATUS_DATA          0x48
#define XCOLOR_DIVIDER_CONTROL_BITS_STATUS_DATA          32
#define XCOLOR_DIVIDER_CONTROL_ADDR_STATUS_CTRL          0x4c
#define XCOLOR_DIVIDER_CONTROL_ADDR_OUTPUT_PIXELS_DATA   0x58
#define XCOLOR_DIVIDER_CONTROL_BITS_OUTPUT_PIXELS_DATA   32
#define XCOLOR_DIVIDER_CONTROL_ADDR_OUTPUT_PIXELS_CTRL   0x5c

