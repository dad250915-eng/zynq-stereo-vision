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
// 0x10 : Data signal of disparity
//        bit 31~0 - disparity[31:0] (Read/Write)
// 0x14 : Data signal of disparity
//        bit 31~0 - disparity[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of mask
//        bit 31~0 - mask[31:0] (Read/Write)
// 0x20 : Data signal of mask
//        bit 31~0 - mask[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of result
//        bit 31~0 - result[31:0] (Read/Write)
// 0x2c : Data signal of result
//        bit 31~0 - result[63:32] (Read/Write)
// 0x30 : reserved
// 0x34 : Data signal of rows
//        bit 31~0 - rows[31:0] (Read/Write)
// 0x38 : reserved
// 0x3c : Data signal of cols
//        bit 31~0 - cols[31:0] (Read/Write)
// 0x40 : reserved
// 0x44 : Data signal of disparity_stride_bytes
//        bit 31~0 - disparity_stride_bytes[31:0] (Read/Write)
// 0x48 : reserved
// 0x4c : Data signal of mask_stride_bytes
//        bit 31~0 - mask_stride_bytes[31:0] (Read/Write)
// 0x50 : reserved
// 0x54 : Data signal of roi_x
//        bit 31~0 - roi_x[31:0] (Read/Write)
// 0x58 : reserved
// 0x5c : Data signal of roi_y
//        bit 31~0 - roi_y[31:0] (Read/Write)
// 0x60 : reserved
// 0x64 : Data signal of roi_width
//        bit 31~0 - roi_width[31:0] (Read/Write)
// 0x68 : reserved
// 0x6c : Data signal of roi_height
//        bit 31~0 - roi_height[31:0] (Read/Write)
// 0x70 : reserved
// 0x74 : Data signal of disparity_frame_id
//        bit 31~0 - disparity_frame_id[31:0] (Read/Write)
// 0x78 : reserved
// 0x7c : Data signal of mask_frame_id
//        bit 31~0 - mask_frame_id[31:0] (Read/Write)
// 0x80 : reserved
// 0x84 : Data signal of min_disparity
//        bit 31~0 - min_disparity[31:0] (Read/Write)
// 0x88 : reserved
// 0x8c : Data signal of max_disparity
//        bit 31~0 - max_disparity[31:0] (Read/Write)
// 0x90 : reserved
// 0x94 : Data signal of min_points
//        bit 31~0 - min_points[31:0] (Read/Write)
// 0x98 : reserved
// 0x9c : Data signal of min_support_permille
//        bit 31~0 - min_support_permille[31:0] (Read/Write)
// 0xa0 : reserved
// 0xa4 : Data signal of max_iqr
//        bit 31~0 - max_iqr[31:0] (Read/Write)
// 0xa8 : reserved
// 0xac : Data signal of numerator_mm_px
//        bit 31~0 - numerator_mm_px[31:0] (Read/Write)
// 0xb0 : reserved
// 0xb4 : Data signal of principal_delta_px
//        bit 31~0 - principal_delta_px[31:0] (Read/Write)
// 0xb8 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XDEPTH_MEASURE_CONTROL_ADDR_AP_CTRL                     0x00
#define XDEPTH_MEASURE_CONTROL_ADDR_GIE                         0x04
#define XDEPTH_MEASURE_CONTROL_ADDR_IER                         0x08
#define XDEPTH_MEASURE_CONTROL_ADDR_ISR                         0x0c
#define XDEPTH_MEASURE_CONTROL_ADDR_DISPARITY_DATA              0x10
#define XDEPTH_MEASURE_CONTROL_BITS_DISPARITY_DATA              64
#define XDEPTH_MEASURE_CONTROL_ADDR_MASK_DATA                   0x1c
#define XDEPTH_MEASURE_CONTROL_BITS_MASK_DATA                   64
#define XDEPTH_MEASURE_CONTROL_ADDR_RESULT_DATA                 0x28
#define XDEPTH_MEASURE_CONTROL_BITS_RESULT_DATA                 64
#define XDEPTH_MEASURE_CONTROL_ADDR_ROWS_DATA                   0x34
#define XDEPTH_MEASURE_CONTROL_BITS_ROWS_DATA                   32
#define XDEPTH_MEASURE_CONTROL_ADDR_COLS_DATA                   0x3c
#define XDEPTH_MEASURE_CONTROL_BITS_COLS_DATA                   32
#define XDEPTH_MEASURE_CONTROL_ADDR_DISPARITY_STRIDE_BYTES_DATA 0x44
#define XDEPTH_MEASURE_CONTROL_BITS_DISPARITY_STRIDE_BYTES_DATA 32
#define XDEPTH_MEASURE_CONTROL_ADDR_MASK_STRIDE_BYTES_DATA      0x4c
#define XDEPTH_MEASURE_CONTROL_BITS_MASK_STRIDE_BYTES_DATA      32
#define XDEPTH_MEASURE_CONTROL_ADDR_ROI_X_DATA                  0x54
#define XDEPTH_MEASURE_CONTROL_BITS_ROI_X_DATA                  32
#define XDEPTH_MEASURE_CONTROL_ADDR_ROI_Y_DATA                  0x5c
#define XDEPTH_MEASURE_CONTROL_BITS_ROI_Y_DATA                  32
#define XDEPTH_MEASURE_CONTROL_ADDR_ROI_WIDTH_DATA              0x64
#define XDEPTH_MEASURE_CONTROL_BITS_ROI_WIDTH_DATA              32
#define XDEPTH_MEASURE_CONTROL_ADDR_ROI_HEIGHT_DATA             0x6c
#define XDEPTH_MEASURE_CONTROL_BITS_ROI_HEIGHT_DATA             32
#define XDEPTH_MEASURE_CONTROL_ADDR_DISPARITY_FRAME_ID_DATA     0x74
#define XDEPTH_MEASURE_CONTROL_BITS_DISPARITY_FRAME_ID_DATA     32
#define XDEPTH_MEASURE_CONTROL_ADDR_MASK_FRAME_ID_DATA          0x7c
#define XDEPTH_MEASURE_CONTROL_BITS_MASK_FRAME_ID_DATA          32
#define XDEPTH_MEASURE_CONTROL_ADDR_MIN_DISPARITY_DATA          0x84
#define XDEPTH_MEASURE_CONTROL_BITS_MIN_DISPARITY_DATA          32
#define XDEPTH_MEASURE_CONTROL_ADDR_MAX_DISPARITY_DATA          0x8c
#define XDEPTH_MEASURE_CONTROL_BITS_MAX_DISPARITY_DATA          32
#define XDEPTH_MEASURE_CONTROL_ADDR_MIN_POINTS_DATA             0x94
#define XDEPTH_MEASURE_CONTROL_BITS_MIN_POINTS_DATA             32
#define XDEPTH_MEASURE_CONTROL_ADDR_MIN_SUPPORT_PERMILLE_DATA   0x9c
#define XDEPTH_MEASURE_CONTROL_BITS_MIN_SUPPORT_PERMILLE_DATA   32
#define XDEPTH_MEASURE_CONTROL_ADDR_MAX_IQR_DATA                0xa4
#define XDEPTH_MEASURE_CONTROL_BITS_MAX_IQR_DATA                32
#define XDEPTH_MEASURE_CONTROL_ADDR_NUMERATOR_MM_PX_DATA        0xac
#define XDEPTH_MEASURE_CONTROL_BITS_NUMERATOR_MM_PX_DATA        32
#define XDEPTH_MEASURE_CONTROL_ADDR_PRINCIPAL_DELTA_PX_DATA     0xb4
#define XDEPTH_MEASURE_CONTROL_BITS_PRINCIPAL_DELTA_PX_DATA     32

