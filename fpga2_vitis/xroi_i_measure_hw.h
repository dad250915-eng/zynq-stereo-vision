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
// 0x10 : Data signal of mask
//        bit 31~0 - mask[31:0] (Read/Write)
// 0x14 : Data signal of mask
//        bit 31~0 - mask[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of meta
//        bit 31~0 - meta[31:0] (Read/Write)
// 0x20 : Data signal of meta
//        bit 31~0 - meta[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of result
//        bit 31~0 - result[31:0] (Read/Write)
// 0x2c : Data signal of result
//        bit 31~0 - result[63:32] (Read/Write)
// 0x30 : reserved
// 0x34 : Data signal of x0
//        bit 31~0 - x0[31:0] (Read/Write)
// 0x38 : reserved
// 0x3c : Data signal of y0
//        bit 31~0 - y0[31:0] (Read/Write)
// 0x40 : reserved
// 0x44 : Data signal of width
//        bit 31~0 - width[31:0] (Read/Write)
// 0x48 : reserved
// 0x4c : Data signal of height
//        bit 31~0 - height[31:0] (Read/Write)
// 0x50 : reserved
// 0x54 : Data signal of frame
//        bit 31~0 - frame[31:0] (Read/Write)
// 0x58 : reserved
// 0x5c : Data signal of mask_frame
//        bit 31~0 - mask_frame[31:0] (Read/Write)
// 0x60 : reserved
// 0x64 : Data signal of min_disparity
//        bit 31~0 - min_disparity[31:0] (Read/Write)
// 0x68 : reserved
// 0x6c : Data signal of max_disparity
//        bit 31~0 - max_disparity[31:0] (Read/Write)
// 0x70 : reserved
// 0x74 : Data signal of min_points
//        bit 31~0 - min_points[31:0] (Read/Write)
// 0x78 : reserved
// 0x7c : Data signal of min_support
//        bit 31~0 - min_support[31:0] (Read/Write)
// 0x80 : reserved
// 0x84 : Data signal of max_iqr
//        bit 31~0 - max_iqr[31:0] (Read/Write)
// 0x88 : reserved
// 0x8c : Data signal of numerator
//        bit 31~0 - numerator[31:0] (Read/Write)
// 0x90 : reserved
// 0x94 : Data signal of delta
//        bit 31~0 - delta[31:0] (Read/Write)
// 0x98 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XROI_I_MEASURE_CONTROL_ADDR_AP_CTRL            0x00
#define XROI_I_MEASURE_CONTROL_ADDR_GIE                0x04
#define XROI_I_MEASURE_CONTROL_ADDR_IER                0x08
#define XROI_I_MEASURE_CONTROL_ADDR_ISR                0x0c
#define XROI_I_MEASURE_CONTROL_ADDR_MASK_DATA          0x10
#define XROI_I_MEASURE_CONTROL_BITS_MASK_DATA          64
#define XROI_I_MEASURE_CONTROL_ADDR_META_DATA          0x1c
#define XROI_I_MEASURE_CONTROL_BITS_META_DATA          64
#define XROI_I_MEASURE_CONTROL_ADDR_RESULT_DATA        0x28
#define XROI_I_MEASURE_CONTROL_BITS_RESULT_DATA        64
#define XROI_I_MEASURE_CONTROL_ADDR_X0_DATA            0x34
#define XROI_I_MEASURE_CONTROL_BITS_X0_DATA            32
#define XROI_I_MEASURE_CONTROL_ADDR_Y0_DATA            0x3c
#define XROI_I_MEASURE_CONTROL_BITS_Y0_DATA            32
#define XROI_I_MEASURE_CONTROL_ADDR_WIDTH_DATA         0x44
#define XROI_I_MEASURE_CONTROL_BITS_WIDTH_DATA         32
#define XROI_I_MEASURE_CONTROL_ADDR_HEIGHT_DATA        0x4c
#define XROI_I_MEASURE_CONTROL_BITS_HEIGHT_DATA        32
#define XROI_I_MEASURE_CONTROL_ADDR_FRAME_DATA         0x54
#define XROI_I_MEASURE_CONTROL_BITS_FRAME_DATA         32
#define XROI_I_MEASURE_CONTROL_ADDR_MASK_FRAME_DATA    0x5c
#define XROI_I_MEASURE_CONTROL_BITS_MASK_FRAME_DATA    32
#define XROI_I_MEASURE_CONTROL_ADDR_MIN_DISPARITY_DATA 0x64
#define XROI_I_MEASURE_CONTROL_BITS_MIN_DISPARITY_DATA 32
#define XROI_I_MEASURE_CONTROL_ADDR_MAX_DISPARITY_DATA 0x6c
#define XROI_I_MEASURE_CONTROL_BITS_MAX_DISPARITY_DATA 32
#define XROI_I_MEASURE_CONTROL_ADDR_MIN_POINTS_DATA    0x74
#define XROI_I_MEASURE_CONTROL_BITS_MIN_POINTS_DATA    32
#define XROI_I_MEASURE_CONTROL_ADDR_MIN_SUPPORT_DATA   0x7c
#define XROI_I_MEASURE_CONTROL_BITS_MIN_SUPPORT_DATA   32
#define XROI_I_MEASURE_CONTROL_ADDR_MAX_IQR_DATA       0x84
#define XROI_I_MEASURE_CONTROL_BITS_MAX_IQR_DATA       32
#define XROI_I_MEASURE_CONTROL_ADDR_NUMERATOR_DATA     0x8c
#define XROI_I_MEASURE_CONTROL_BITS_NUMERATOR_DATA     32
#define XROI_I_MEASURE_CONTROL_ADDR_DELTA_DATA         0x94
#define XROI_I_MEASURE_CONTROL_BITS_DELTA_DATA         32

