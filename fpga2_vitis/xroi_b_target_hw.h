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
// 0x10 : Data signal of image_r
//        bit 31~0 - image_r[31:0] (Read/Write)
// 0x14 : Data signal of image_r
//        bit 31~0 - image_r[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of work_mask
//        bit 31~0 - work_mask[31:0] (Read/Write)
// 0x20 : Data signal of work_mask
//        bit 31~0 - work_mask[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of queue
//        bit 31~0 - queue[31:0] (Read/Write)
// 0x2c : Data signal of queue
//        bit 31~0 - queue[63:32] (Read/Write)
// 0x30 : reserved
// 0x34 : Data signal of meta
//        bit 31~0 - meta[31:0] (Read/Write)
// 0x38 : Data signal of meta
//        bit 31~0 - meta[63:32] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of rows
//        bit 31~0 - rows[31:0] (Read/Write)
// 0x44 : reserved
// 0x48 : Data signal of cols
//        bit 31~0 - cols[31:0] (Read/Write)
// 0x4c : reserved
// 0x50 : Data signal of color_stride_words
//        bit 31~0 - color_stride_words[31:0] (Read/Write)
// 0x54 : reserved
// 0x58 : Data signal of low_r
//        bit 31~0 - low_r[31:0] (Read/Write)
// 0x5c : reserved
// 0x60 : Data signal of high_r
//        bit 31~0 - high_r[31:0] (Read/Write)
// 0x64 : reserved
// 0x68 : Data signal of min_area
//        bit 31~0 - min_area[31:0] (Read/Write)
// 0x6c : reserved
// 0x70 : Data signal of max_area
//        bit 31~0 - max_area[31:0] (Read/Write)
// 0x74 : reserved
// 0x78 : Data signal of frame
//        bit 31~0 - frame[31:0] (Read/Write)
// 0x7c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XROI_B_TARGET_CONTROL_ADDR_AP_CTRL                 0x00
#define XROI_B_TARGET_CONTROL_ADDR_GIE                     0x04
#define XROI_B_TARGET_CONTROL_ADDR_IER                     0x08
#define XROI_B_TARGET_CONTROL_ADDR_ISR                     0x0c
#define XROI_B_TARGET_CONTROL_ADDR_IMAGE_R_DATA            0x10
#define XROI_B_TARGET_CONTROL_BITS_IMAGE_R_DATA            64
#define XROI_B_TARGET_CONTROL_ADDR_WORK_MASK_DATA          0x1c
#define XROI_B_TARGET_CONTROL_BITS_WORK_MASK_DATA          64
#define XROI_B_TARGET_CONTROL_ADDR_QUEUE_DATA              0x28
#define XROI_B_TARGET_CONTROL_BITS_QUEUE_DATA              64
#define XROI_B_TARGET_CONTROL_ADDR_META_DATA               0x34
#define XROI_B_TARGET_CONTROL_BITS_META_DATA               64
#define XROI_B_TARGET_CONTROL_ADDR_ROWS_DATA               0x40
#define XROI_B_TARGET_CONTROL_BITS_ROWS_DATA               32
#define XROI_B_TARGET_CONTROL_ADDR_COLS_DATA               0x48
#define XROI_B_TARGET_CONTROL_BITS_COLS_DATA               32
#define XROI_B_TARGET_CONTROL_ADDR_COLOR_STRIDE_WORDS_DATA 0x50
#define XROI_B_TARGET_CONTROL_BITS_COLOR_STRIDE_WORDS_DATA 32
#define XROI_B_TARGET_CONTROL_ADDR_LOW_R_DATA              0x58
#define XROI_B_TARGET_CONTROL_BITS_LOW_R_DATA              32
#define XROI_B_TARGET_CONTROL_ADDR_HIGH_R_DATA             0x60
#define XROI_B_TARGET_CONTROL_BITS_HIGH_R_DATA             32
#define XROI_B_TARGET_CONTROL_ADDR_MIN_AREA_DATA           0x68
#define XROI_B_TARGET_CONTROL_BITS_MIN_AREA_DATA           32
#define XROI_B_TARGET_CONTROL_ADDR_MAX_AREA_DATA           0x70
#define XROI_B_TARGET_CONTROL_BITS_MAX_AREA_DATA           32
#define XROI_B_TARGET_CONTROL_ADDR_FRAME_DATA              0x78
#define XROI_B_TARGET_CONTROL_BITS_FRAME_DATA              32

