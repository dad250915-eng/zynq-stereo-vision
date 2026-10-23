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
// 0x10 : Data signal of rgb565
//        bit 31~0 - rgb565[31:0] (Read/Write)
// 0x14 : Data signal of rgb565
//        bit 31~0 - rgb565[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of palette
//        bit 31~0 - palette[31:0] (Read/Write)
// 0x20 : Data signal of palette
//        bit 31~0 - palette[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of mode
//        bit 31~0 - mode[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of rows
//        bit 31~0 - rows[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of cols
//        bit 31~0 - cols[31:0] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of errors
//        bit 31~0 - errors[31:0] (Read)
// 0x44 : Control signal of errors
//        bit 0  - errors_ap_vld (Read/COR)
//        others - reserved
// 0x50 : Data signal of image_r
//        bit 31~0 - image_r[31:0] (Read/Write)
// 0x54 : Data signal of image_r
//        bit 31~0 - image_r[63:32] (Read/Write)
// 0x58 : reserved
// 0x5c : Data signal of color_stride_words
//        bit 31~0 - color_stride_words[31:0] (Read/Write)
// 0x60 : reserved
// 0x64 : Data signal of blend_eighths
//        bit 31~0 - blend_eighths[31:0] (Read/Write)
// 0x68 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XROI_K_DEPTH_CONTROL_ADDR_AP_CTRL                 0x00
#define XROI_K_DEPTH_CONTROL_ADDR_GIE                     0x04
#define XROI_K_DEPTH_CONTROL_ADDR_IER                     0x08
#define XROI_K_DEPTH_CONTROL_ADDR_ISR                     0x0c
#define XROI_K_DEPTH_CONTROL_ADDR_RGB565_DATA             0x10
#define XROI_K_DEPTH_CONTROL_BITS_RGB565_DATA             64
#define XROI_K_DEPTH_CONTROL_ADDR_PALETTE_DATA            0x1c
#define XROI_K_DEPTH_CONTROL_BITS_PALETTE_DATA            64
#define XROI_K_DEPTH_CONTROL_ADDR_MODE_DATA               0x28
#define XROI_K_DEPTH_CONTROL_BITS_MODE_DATA               32
#define XROI_K_DEPTH_CONTROL_ADDR_ROWS_DATA               0x30
#define XROI_K_DEPTH_CONTROL_BITS_ROWS_DATA               32
#define XROI_K_DEPTH_CONTROL_ADDR_COLS_DATA               0x38
#define XROI_K_DEPTH_CONTROL_BITS_COLS_DATA               32
#define XROI_K_DEPTH_CONTROL_ADDR_ERRORS_DATA             0x40
#define XROI_K_DEPTH_CONTROL_BITS_ERRORS_DATA             32
#define XROI_K_DEPTH_CONTROL_ADDR_ERRORS_CTRL             0x44
#define XROI_K_DEPTH_CONTROL_ADDR_IMAGE_R_DATA            0x50
#define XROI_K_DEPTH_CONTROL_BITS_IMAGE_R_DATA            64
#define XROI_K_DEPTH_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA 0x5c
#define XROI_K_DEPTH_CONTROL_BITS_COLOR_STRIDE_WORDS_DATA 32
#define XROI_K_DEPTH_CONTROL_ADDR_BLEND_EIGHTHS_DATA      0x64
#define XROI_K_DEPTH_CONTROL_BITS_BLEND_EIGHTHS_DATA      32

