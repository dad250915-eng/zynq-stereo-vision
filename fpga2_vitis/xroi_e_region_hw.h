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
// 0x10 : Data signal of meta
//        bit 31~0 - meta[31:0] (Read/Write)
// 0x14 : Data signal of meta
//        bit 31~0 - meta[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of descriptor
//        bit 31~0 - descriptor[31:0] (Read/Write)
// 0x20 : Data signal of descriptor
//        bit 31~0 - descriptor[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of max_disparity
//        bit 31~0 - max_disparity[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of context_r
//        bit 31~0 - context_r[31:0] (Read/Write)
// 0x34 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XROI_E_REGION_CONTROL_ADDR_AP_CTRL            0x00
#define XROI_E_REGION_CONTROL_ADDR_GIE                0x04
#define XROI_E_REGION_CONTROL_ADDR_IER                0x08
#define XROI_E_REGION_CONTROL_ADDR_ISR                0x0c
#define XROI_E_REGION_CONTROL_ADDR_META_DATA          0x10
#define XROI_E_REGION_CONTROL_BITS_META_DATA          64
#define XROI_E_REGION_CONTROL_ADDR_DESCRIPTOR_DATA    0x1c
#define XROI_E_REGION_CONTROL_BITS_DESCRIPTOR_DATA    64
#define XROI_E_REGION_CONTROL_ADDR_MAX_DISPARITY_DATA 0x28
#define XROI_E_REGION_CONTROL_BITS_MAX_DISPARITY_DATA 32
#define XROI_E_REGION_CONTROL_ADDR_CONTEXT_R_DATA     0x30
#define XROI_E_REGION_CONTROL_BITS_CONTEXT_R_DATA     32

