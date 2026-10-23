/************************************************************************/
/*																		*/
/*	display_demo.c	--	ALINX AX7010 HDMI Display demonstration 						*/
/*																		*/
/************************************************************************/

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "display_demo.h"
#include "display_ctrl/display_ctrl.h"
#include <stdio.h>
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xtime_l.h"
#include "xparameters.h"
#include "vdma.h"


#include "sleep.h"

#include "stereo_match_hw.h"
#include "rx_control.h"
#include "sgm2_config.h"

/* 1: corrected binocular display; 0: stereo disparity display. */
#ifndef CORRECTED_BINOCULAR_DISPLAY
#define CORRECTED_BINOCULAR_DISPLAY 0
#endif
/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_S_AXI_LITE_BASEADDR
#define VGA_VDMA_ID XPAR_AXI_VDMA_2_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_IRPT_ID XPS_FPGA3_INT_ID
#define VID_GPIO_IRPT_ID XPS_FPGA4_INT_ID
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR STDIN_BASEADDRESS

/* Stereo matcher and the three software-controlled AXIS switches. */
#if defined(XPAR_SGM_STEREO_0_S_AXI_BASEADDR)
#define STEREO_MATCH_BASEADDR XPAR_SGM_STEREO_0_S_AXI_BASEADDR
#elif defined(XPAR_SGM_STEREO_0_BASEADDR)
#define STEREO_MATCH_BASEADDR XPAR_SGM_STEREO_0_BASEADDR
#elif defined(XPAR_STEREO_MATCH_V1_0_0_BASEADDR)
#define STEREO_MATCH_BASEADDR XPAR_STEREO_MATCH_V1_0_0_BASEADDR
#else
#define STEREO_MATCH_BASEADDR 0x40000000U
#endif

#if defined(XPAR_AXIS_SWITCH_0_S_AXI_CTRL_BASEADDR)
#define LEFT_INPUT_SWITCH_BASEADDR XPAR_AXIS_SWITCH_0_S_AXI_CTRL_BASEADDR
#elif defined(XPAR_AXIS_SWITCH_0_BASEADDR)
#define LEFT_INPUT_SWITCH_BASEADDR XPAR_AXIS_SWITCH_0_BASEADDR
#else
#define LEFT_INPUT_SWITCH_BASEADDR 0x43C60000U
#endif

#if defined(XPAR_AXIS_SWITCH_1_S_AXI_CTRL_BASEADDR)
#define RIGHT_INPUT_SWITCH_BASEADDR XPAR_AXIS_SWITCH_1_S_AXI_CTRL_BASEADDR
#elif defined(XPAR_AXIS_SWITCH_1_BASEADDR)
#define RIGHT_INPUT_SWITCH_BASEADDR XPAR_AXIS_SWITCH_1_BASEADDR
#else
#define RIGHT_INPUT_SWITCH_BASEADDR 0x43C70000U
#endif

#if defined(XPAR_AXIS_SWITCH_2_S_AXI_CTRL_BASEADDR)
#define DISPLAY_SWITCH_BASEADDR XPAR_AXIS_SWITCH_2_S_AXI_CTRL_BASEADDR
#elif defined(XPAR_AXIS_SWITCH_2_BASEADDR)
#define DISPLAY_SWITCH_BASEADDR XPAR_AXIS_SWITCH_2_BASEADDR
#else
/* Address Editor must assign axis_switch_2 before exporting the XSA. */
#define DISPLAY_SWITCH_BASEADDR 0x43C80000U
#endif

#if defined(XPAR_AXI_VDMA_4_DEVICE_ID)
#define DISPARITY_VDMA_DEVICE_ID XPAR_AXI_VDMA_4_DEVICE_ID
#else
#define DISPARITY_VDMA_DEVICE_ID 4U
#endif

#define AXIS_SWITCH_CTRL          0x00U
#define AXIS_SWITCH_MI_MUX_BASE   0x40U
#define AXIS_SWITCH_MI_MUX_STEP   0x04U
#define AXIS_SWITCH_REG_UPDATE    0x02U
#define AXIS_SWITCH_MI_DISABLE    0x80000000U

#define DISPARITY_WIDTH           1280U
#define DISPARITY_HEIGHT          720U
#define DISPARITY_BYTES_PER_PIXEL 2U
#define DISPARITY_STRIDE          (DISPARITY_WIDTH * DISPARITY_BYTES_PER_PIXEL)
#define DISPARITY_FRAME_BYTES     (DISPARITY_STRIDE * DISPARITY_HEIGHT)
#define DISPARITY_FRAME_COUNT     3U
#define DISPARITY_FRAME0_ADDR     0x07000000U
#define DISPARITY_FRAME1_ADDR     0x07200000U
#define DISPARITY_FRAME2_ADDR     0x07400000U
#define STEREO_TIMEOUT_US         2000000U

/* Rectification tables are dense 1280 x 720 arrays (no 1920-pixel padding). */
#define RECTIFY_MAP_WIDTH       1280U
#define RECTIFY_MAP_HEIGHT      720U
#define RECTIFY_MAP_BYTES       (RECTIFY_MAP_WIDTH * RECTIFY_MAP_HEIGHT * 4U)
#define RECTIFY_OUTPUT_BYTES    (1920U * 720U * 4U)
#define RECTIFY_OUTPUT_FRAMES   3U
#define RECTIFY_STORAGE_REGIONS (1U + 2U * RECTIFY_OUTPUT_FRAMES)

/*
 * Keep this fallback equal to the address assigned to remap_operate_0/s_axi
 * in Vivado Address Editor.  A generated xparameters definition is preferred
 * after the new XSA has been imported.
 */
#if defined(XPAR_REMAP_OPERATE_PIPELI_0_BASEADDR)
#define REMAP_ACCEL_LEFT_BASEADDR XPAR_REMAP_OPERATE_PIPELI_0_BASEADDR
#elif defined(XPAR_REMAP_OPERATE_PIPELINE_V1_0_BASEADDR)
#define REMAP_ACCEL_LEFT_BASEADDR XPAR_REMAP_OPERATE_PIPELINE_V1_0_BASEADDR
#else
#define REMAP_ACCEL_LEFT_BASEADDR 0x43C10000U
#endif

#if defined(XPAR_REMAP_OPERATE_PIPELI_1_BASEADDR)
#define REMAP_ACCEL_RIGHT_BASEADDR XPAR_REMAP_OPERATE_PIPELI_1_BASEADDR
#else
#define REMAP_ACCEL_RIGHT_BASEADDR 0x43C40000U
#endif

#if defined(XPAR_REMAP_GENERATE_V1_0_0_BASEADDR)
#define REMAP_GENERATE_LEFT_BASEADDR XPAR_REMAP_GENERATE_V1_0_0_BASEADDR
#else
#define REMAP_GENERATE_LEFT_BASEADDR 0x43C30000U
#endif

#if defined(XPAR_REMAP_GENERATE_V1_0_1_BASEADDR)
#define REMAP_GENERATE_RIGHT_BASEADDR XPAR_REMAP_GENERATE_V1_0_1_BASEADDR
#else
#define REMAP_GENERATE_RIGHT_BASEADDR 0x43C50000U
#endif

#define REMAP_REG_CONTROL 0x00U
#define REMAP_REG_SOURCE  0x04U
#define REMAP_REG_MAP     0x08U
#define REMAP_REG_DEST    0x0CU
#define REMAP_REG_WIDTH   0x10U
#define REMAP_REG_HEIGHT  0x14U
#define REMAP_REG_STRIDE  0x18U
#define REMAP_REG_STATE   0x1CU
#define REMAP_REG_ERROR   0x20U
#define REMAP_REG_TILE_X  0x24U
#define REMAP_REG_TILE_Y  0x28U
#define REMAP_REG_MAP_DBG 0x2CU
#define REMAP_REG_SRC_DBG 0x30U
#define REMAP_REG_OUT_DBG 0x34U
#define REMAP_REG_AXI_DBG 0x38U

#define GENERATE_REG_CONTROL     0x00U
#define GENERATE_REG_SOURCE      0x04U
#define GENERATE_REG_STRIDE      0x08U
#define GENERATE_REG_DESC_BASE   0x0CU
#define GENERATE_REG_TILE_COUNT  0x10U
#define GENERATE_REG_DESC_INDEX  0x14U
#define GENERATE_REG_DESC_W0     0x18U
#define GENERATE_REG_DESC_W1     0x1CU
#define GENERATE_REG_DESC_W2     0x20U
#define GENERATE_REG_DESC_W3     0x24U
#define GENERATE_REG_DESC_COMMIT 0x28U
#define GENERATE_REG_STATE       0x2CU
#define GENERATE_REG_ERROR       0x30U
#define GENERATE_REG_PROGRESS    0x34U

#define RECTIFY_TILE_WIDTH       64U
#define RECTIFY_TILE_HEIGHT      16U
#define RECTIFY_TILES_X          (RECTIFY_MAP_WIDTH / RECTIFY_TILE_WIDTH)
#define RECTIFY_TILES_Y          (RECTIFY_MAP_HEIGHT / RECTIFY_TILE_HEIGHT)
#define RECTIFY_TILE_COUNT       (RECTIFY_TILES_X * RECTIFY_TILES_Y)
#define LEFT_DESCRIPTOR_BASE     0U
#define RIGHT_DESCRIPTOR_BASE    0U
#define SOURCE_BUFFER_PIXELS     4096U

#define REMAP_CONTROL_DONE  (1U << 1)
#define REMAP_CONTROL_BUSY  (1U << 2)
#define REMAP_CONTROL_ERROR (1U << 3)
#define REMAP_BENCHMARK_RUNS 3U
#define REMAP_POLL_TIMEOUT_US 5000000U
#define REMAP_POLL_INTERVAL_US 1000U

static void remap_accel_print_debug(u32 base, const char *name, u32 control)
{
	xil_printf("%s remap debug: control=0x%08X state=%d error_code=%d\r\n",
		name,
		control,
		Xil_In32(base + REMAP_REG_STATE),
		Xil_In32(base + REMAP_REG_ERROR));
	xil_printf("  tile=(%d,%d) map=0x%08X source=0x%08X\r\n",
		Xil_In32(base + REMAP_REG_TILE_X),
		Xil_In32(base + REMAP_REG_TILE_Y),
		Xil_In32(base + REMAP_REG_MAP_DBG),
		Xil_In32(base + REMAP_REG_SRC_DBG));
	xil_printf("  output=0x%08X axi=0x%08X\r\n",
		Xil_In32(base + REMAP_REG_OUT_DBG),
		Xil_In32(base + REMAP_REG_AXI_DBG));
}

static void generate_accel_print_debug(u32 base, const char *name, u32 control)
{
	xil_printf("%s generate debug: control=0x%08X state=%d error=%d progress=%d\r\n",
		name,
		control,
		Xil_In32(base + GENERATE_REG_STATE),
		Xil_In32(base + GENERATE_REG_ERROR),
		Xil_In32(base + GENERATE_REG_PROGRESS));
}

static int program_tile_descriptors(const u32 *map, u32 generate_base,
		u32 descriptor_base, const char *name)
{
	u32 ty, tx, descriptor = descriptor_base;
	u32 max_pixels = 0U;

	for (ty = 0U; ty < RECTIFY_MAP_HEIGHT; ty += RECTIFY_TILE_HEIGHT) {
		for (tx = 0U; tx < RECTIFY_MAP_WIDTH; tx += RECTIFY_TILE_WIDTH) {
			u32 y, x;
			u32 min_x = 0xFFFFU, min_y = 0xFFFFU;
			u32 max_x = 0U, max_y = 0U;
			u32 valid = 0U;
			u32 x0 = 0U, y0 = 0U, width = 0U, height = 0U;

			for (y = ty; y < ty + RECTIFY_TILE_HEIGHT; y++) {
				for (x = tx; x < tx + RECTIFY_TILE_WIDTH; x++) {
					u32 packed = map[y * RECTIFY_MAP_WIDTH + x];
					u32 sx, sy;
					if (packed == 0xFFFFFFFFU)
						continue;
					sx = packed & 0xFFFFU;
					sy = packed >> 16;
					if (sx < min_x) min_x = sx;
					if (sx > max_x) max_x = sx;
					if (sy < min_y) min_y = sy;
					if (sy > max_y) max_y = sy;
					valid = 1U;
				}
			}

			if (valid) {
				u32 x1;
				x0 = min_x & ~1U;
				x1 = max_x | 1U;
				y0 = min_y;
				width = x1 - x0 + 1U;
				height = max_y - min_y + 1U;
				if (width * height > SOURCE_BUFFER_PIXELS) {
					xil_printf("%s descriptor %d exceeds source BRAM: %d pixels\r\n",
						name, descriptor - descriptor_base, width * height);
					return XST_FAILURE;
				}
				if (width * height > max_pixels)
					max_pixels = width * height;
			}

			Xil_Out32(generate_base + GENERATE_REG_DESC_INDEX,
				descriptor);
			Xil_Out32(generate_base + GENERATE_REG_DESC_W0,
				(y0 << 16) | x0);
			Xil_Out32(generate_base + GENERATE_REG_DESC_W1,
				(height << 16) | width);
			Xil_Out32(generate_base + GENERATE_REG_DESC_W2,
				(ty << 16) | tx);
			Xil_Out32(generate_base + GENERATE_REG_DESC_W3, 0U);
			Xil_Out32(generate_base + GENERATE_REG_DESC_COMMIT, 1U);
			descriptor++;
		}
	}

	xil_printf("%s descriptors programmed: %d, maximum rectangle=%d pixels\r\n",
		name, RECTIFY_TILE_COUNT, max_pixels);
	return XST_SUCCESS;
}

/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

/*
 * Display Driver structs
 */
DisplayCtrl dispCtrl;
XAxiVdma *vdma[DISPLAY_NUM_VDMA];
u32 stride[DISPLAY_NUM_VDMA];



XAxiVdma cam_vdma0, cam_vdma1;
/*
 * Framebuffers for video data
 */
__attribute__ ((aligned(8))) u8 frameBuf[DISPLAY_NUM_VDMA][DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME];

u8 *pFrames[DISPLAY_NUM_VDMA][DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

/*
 * Additional DDR layout for the rectification experiment.
 *
 * rectifyStorage[0]: left map followed immediately by right map
 * rectifyStorage[1]: left corrected image, using the existing 1920-pixel stride
 * rectifyStorage[2]: right corrected image, using the existing 1920-pixel stride
 *
 * The original two-camera, three-frame buffers above remain unchanged.
 */
__attribute__ ((aligned(64))) u8
	rectifyStorage[RECTIFY_STORAGE_REGIONS][DEMO_MAX_FRAME];

u32 *leftRectifyMap;
u32 *rightRectifyMap;
u8 *leftCorrectedFrame[RECTIFY_OUTPUT_FRAMES];
u8 *rightCorrectedFrame[RECTIFY_OUTPUT_FRAMES];

static void remap_configure_lane(u32 remap_base, u32 generate_base,
		u32 source_addr, u32 map_addr, u32 dest_addr, u32 descriptor_base)
{
	Xil_Out32(remap_base + REMAP_REG_SOURCE, source_addr);
	Xil_Out32(remap_base + REMAP_REG_MAP, map_addr);
	Xil_Out32(remap_base + REMAP_REG_DEST, dest_addr);
	Xil_Out32(remap_base + REMAP_REG_WIDTH, RECTIFY_MAP_WIDTH);
	Xil_Out32(remap_base + REMAP_REG_HEIGHT, RECTIFY_MAP_HEIGHT);
	Xil_Out32(remap_base + REMAP_REG_STRIDE, 1920U * 4U);

	Xil_Out32(generate_base + GENERATE_REG_SOURCE, source_addr);
	Xil_Out32(generate_base + GENERATE_REG_STRIDE, 1920U * 4U);
	Xil_Out32(generate_base + GENERATE_REG_DESC_BASE, descriptor_base);
	Xil_Out32(generate_base + GENERATE_REG_TILE_COUNT,
		RECTIFY_TILE_COUNT);
}

static u32 remap_elapsed_us(XTime begin, XTime end)
{
	return (u32)(((u64)(end - begin) * 1000000ULL) /
		COUNTS_PER_SECOND);
}

#if SGM2_CONTINUOUS && ROI_ARCHITECTURE
static int stream_capture_protected(void);
#endif

static int remap_stereo_run(u32 left_source, u32 right_source,
		u32 left_destination, u32 right_destination,
		u32 *left_us, u32 *right_us, u32 *pair_us)
{
	u32 left_control = 0U, right_control = 0U;
	u32 left_generate = 0U, right_generate = 0U;
	u32 left_started = 0U, right_started = 0U;
	u32 left_finished = 0U, right_finished = 0U;
	XTime begin, now, left_end = 0, right_end = 0;

	remap_configure_lane(REMAP_ACCEL_LEFT_BASEADDR,
		REMAP_GENERATE_LEFT_BASEADDR, left_source, (u32)leftRectifyMap,
		left_destination, LEFT_DESCRIPTOR_BASE);
	remap_configure_lane(REMAP_ACCEL_RIGHT_BASEADDR,
		REMAP_GENERATE_RIGHT_BASEADDR, right_source, (u32)rightRectifyMap,
		right_destination, RIGHT_DESCRIPTOR_BASE);

	XTime_GetTime(&begin);
	/* Both consumers must be running before either producer emits AXI Stream. */
	Xil_Out32(REMAP_ACCEL_LEFT_BASEADDR + REMAP_REG_CONTROL, 1U);
	Xil_Out32(REMAP_ACCEL_RIGHT_BASEADDR + REMAP_REG_CONTROL, 1U);
	Xil_Out32(REMAP_GENERATE_LEFT_BASEADDR + GENERATE_REG_CONTROL, 1U);
	Xil_Out32(REMAP_GENERATE_RIGHT_BASEADDR + GENERATE_REG_CONTROL, 1U);

	/*
	 * DONE remains high from the preceding benchmark run until a new START is
	 * accepted.  Observe BUSY or DONE-low independently on both lanes before
	 * treating a DONE bit as completion of this run.
	 */
	while (!(left_finished && right_finished)) {
#if SGM2_CONTINUOUS && ROI_ARCHITECTURE
        if(stream_capture_protected()!=XST_SUCCESS){
            xil_printf("REMAP STOP: raw buffer protection lost; output rejected.\r\n");
            return XST_FAILURE;
        }
#endif
		left_control = Xil_In32(REMAP_ACCEL_LEFT_BASEADDR + REMAP_REG_CONTROL);
		right_control = Xil_In32(REMAP_ACCEL_RIGHT_BASEADDR + REMAP_REG_CONTROL);
		left_generate = Xil_In32(REMAP_GENERATE_LEFT_BASEADDR +
			GENERATE_REG_CONTROL);
		right_generate = Xil_In32(REMAP_GENERATE_RIGHT_BASEADDR +
			GENERATE_REG_CONTROL);

		if ((left_control & REMAP_CONTROL_BUSY) ||
			!(left_control & REMAP_CONTROL_DONE)) left_started = 1U;
		if ((right_control & REMAP_CONTROL_BUSY) ||
			!(right_control & REMAP_CONTROL_DONE)) right_started = 1U;

		if (left_control & REMAP_CONTROL_ERROR) {
			remap_accel_print_debug(REMAP_ACCEL_LEFT_BASEADDR, "LEFT",
				left_control);
			return XST_FAILURE;
		}
		if (right_control & REMAP_CONTROL_ERROR) {
			remap_accel_print_debug(REMAP_ACCEL_RIGHT_BASEADDR, "RIGHT",
				right_control);
			return XST_FAILURE;
		}
		if (left_generate & REMAP_CONTROL_ERROR) {
			generate_accel_print_debug(REMAP_GENERATE_LEFT_BASEADDR, "LEFT",
				left_generate);
			return XST_FAILURE;
		}
		if (right_generate & REMAP_CONTROL_ERROR) {
			generate_accel_print_debug(REMAP_GENERATE_RIGHT_BASEADDR, "RIGHT",
				right_generate);
			return XST_FAILURE;
		}

		if (left_started && !left_finished &&
			(left_control & REMAP_CONTROL_DONE)) {
			XTime_GetTime(&left_end);
			left_finished = 1U;
		}
		if (right_started && !right_finished &&
			(right_control & REMAP_CONTROL_DONE)) {
			XTime_GetTime(&right_end);
			right_finished = 1U;
		}

		XTime_GetTime(&now);
		if (remap_elapsed_us(begin, now) >= REMAP_POLL_TIMEOUT_US) {
			xil_printf("Parallel remap timeout after %d us\r\n",
				REMAP_POLL_TIMEOUT_US);
			remap_accel_print_debug(REMAP_ACCEL_LEFT_BASEADDR, "LEFT",
				left_control);
			remap_accel_print_debug(REMAP_ACCEL_RIGHT_BASEADDR, "RIGHT",
				right_control);
			generate_accel_print_debug(REMAP_GENERATE_LEFT_BASEADDR, "LEFT",
				left_generate);
			generate_accel_print_debug(REMAP_GENERATE_RIGHT_BASEADDR, "RIGHT",
				right_generate);
			return XST_FAILURE;
		}
		if (!(left_finished && right_finished)) usleep(100U);
	}

	if ((left_generate & REMAP_CONTROL_DONE) == 0U ||
		(left_generate & REMAP_CONTROL_ERROR)) {
		generate_accel_print_debug(REMAP_GENERATE_LEFT_BASEADDR, "LEFT",
			left_generate);
		return XST_FAILURE;
	}
	if ((right_generate & REMAP_CONTROL_DONE) == 0U ||
		(right_generate & REMAP_CONTROL_ERROR)) {
		generate_accel_print_debug(REMAP_GENERATE_RIGHT_BASEADDR, "RIGHT",
			right_generate);
		return XST_FAILURE;
	}

	*left_us = remap_elapsed_us(begin, left_end);
	*right_us = remap_elapsed_us(begin, right_end);
	*pair_us = (*left_us > *right_us) ? *left_us : *right_us;

	return XST_SUCCESS;
}

static void display_set_corrected_slot(DisplayCtrl *display, u32 slot,
		u8 *left_frame, u8 *right_frame)
{
	u32 offset = XAXIVDMA_MM2S_ADDR_OFFSET + XAXIVDMA_START_ADDR_OFFSET +
		slot * XAXIVDMA_START_ADDR_LEN;

	/* Update only an inactive frame-store register.  MM2S and the downstream
	 * two-input OSD remain running, so neither AXI Stream is cut mid-frame. */
	XAxiVdma_WriteReg(display->vdma[0]->BaseAddr, offset, (u32)left_frame);
	XAxiVdma_WriteReg(display->vdma[1]->BaseAddr, offset, (u32)right_frame);
	display->framePtr[0][slot] = left_frame;
	display->framePtr[1][slot] = right_frame;
	display->vdmaConfig[0].FrameStoreStartAddr[slot] = (u32)left_frame;
	display->vdmaConfig[1].FrameStoreStartAddr[slot] = (u32)right_frame;
	/* Commit updated start addresses; PG020 applies them at a frame boundary. */
	XAxiVdma_WriteReg(display->vdma[0]->BaseAddr,
		XAXIVDMA_MM2S_ADDR_OFFSET + XAXIVDMA_VSIZE_OFFSET,
		display->VertSizeInput[0]);
	XAxiVdma_WriteReg(display->vdma[1]->BaseAddr,
		XAXIVDMA_MM2S_ADDR_OFFSET + XAXIVDMA_VSIZE_OFFSET,
		display->VertSizeInput[1]);
}

static int display_park_corrected_pair(DisplayCtrl *display, u32 frame_index)
{
	int status;

	status = XAxiVdma_StartParking(display->vdma[0], frame_index,
		XAXIVDMA_READ);
	if (status != XST_SUCCESS) return status;
	status = XAxiVdma_StartParking(display->vdma[1], frame_index,
		XAXIVDMA_READ);
	if (status != XST_SUCCESS) return status;

	display->curFrame[0] = frame_index;
	display->curFrame[1] = frame_index;
	return XST_SUCCESS;
}

static int display_wait_for_pair(DisplayCtrl *display, u32 frame_index)
{
	u32 waited_us = 0U;

	while (XAxiVdma_CurrFrameStore(display->vdma[0], XAXIVDMA_READ) !=
			frame_index ||
		XAxiVdma_CurrFrameStore(display->vdma[1], XAXIVDMA_READ) !=
			frame_index) {
		if (waited_us >= 100000U) {
			xil_printf("Display park timeout: request=%d current=(%d,%d)\r\n",
				frame_index,
				XAxiVdma_CurrFrameStore(display->vdma[0], XAXIVDMA_READ),
				XAxiVdma_CurrFrameStore(display->vdma[1], XAXIVDMA_READ));
			return XST_FAILURE;
		}
		usleep(100U);
		waited_us += 100U;
	}

	return XST_SUCCESS;
}

static void axis_switch_select_binocular_path(void)
{
    const u32 inputs[2] = {LEFT_INPUT_SWITCH_BASEADDR, RIGHT_INPUT_SWITCH_BASEADDR};
    unsigned int i;
    for (i = 0; i < 2; ++i) {
        Xil_Out32(inputs[i] + AXIS_SWITCH_MI_MUX_BASE, 0U);
        Xil_Out32(inputs[i] + AXIS_SWITCH_MI_MUX_BASE + AXIS_SWITCH_MI_MUX_STEP,
            AXIS_SWITCH_MI_DISABLE);
        Xil_Out32(inputs[i] + AXIS_SWITCH_CTRL, AXIS_SWITCH_REG_UPDATE);
    }
    Xil_Out32(DISPLAY_SWITCH_BASEADDR + AXIS_SWITCH_MI_MUX_BASE, 0U);
    Xil_Out32(DISPLAY_SWITCH_BASEADDR + AXIS_SWITCH_CTRL, AXIS_SWITCH_REG_UPDATE);
}

static void axis_switch_select_stereo_path(void)
{
	/*
	 * axis_switch_0/1 are 1-input, 2-output switches:
	 *   M00 -> original two-camera OSD path
	 *   M01 -> stereo matcher
	 * Disable M00 and route S00 to M01 on both corrected-image streams.
	 */
	Xil_Out32(LEFT_INPUT_SWITCH_BASEADDR + AXIS_SWITCH_MI_MUX_BASE,
		AXIS_SWITCH_MI_DISABLE);
	Xil_Out32(LEFT_INPUT_SWITCH_BASEADDR + AXIS_SWITCH_MI_MUX_BASE +
		AXIS_SWITCH_MI_MUX_STEP, 0U);
	Xil_Out32(LEFT_INPUT_SWITCH_BASEADDR + AXIS_SWITCH_CTRL,
		AXIS_SWITCH_REG_UPDATE);

	Xil_Out32(RIGHT_INPUT_SWITCH_BASEADDR + AXIS_SWITCH_MI_MUX_BASE,
		AXIS_SWITCH_MI_DISABLE);
	Xil_Out32(RIGHT_INPUT_SWITCH_BASEADDR + AXIS_SWITCH_MI_MUX_BASE +
		AXIS_SWITCH_MI_MUX_STEP, 0U);
	Xil_Out32(RIGHT_INPUT_SWITCH_BASEADDR + AXIS_SWITCH_CTRL,
		AXIS_SWITCH_REG_UPDATE);

	/* axis_switch_2 is a 2-input, 1-output switch.  S01 is disparity. */
	Xil_Out32(DISPLAY_SWITCH_BASEADDR + AXIS_SWITCH_MI_MUX_BASE, 1U);
	Xil_Out32(DISPLAY_SWITCH_BASEADDR + AXIS_SWITCH_CTRL,
		AXIS_SWITCH_REG_UPDATE);
}

#if !SGM2_ENABLED || CORRECTED_BINOCULAR_DISPLAY || !SGM2_FROZEN_OVERLAY
static int disparity_vdma_initialize(XAxiVdma *disparity_vdma,
		const u32 frame_addr[DISPARITY_FRAME_COUNT])
{
	XAxiVdma_Config *config;
	XAxiVdma_DmaSetup read_cfg;
	int status;
	u32 i;

	memset(&read_cfg, 0, sizeof(read_cfg));
	config = XAxiVdma_LookupConfig(DISPARITY_VDMA_DEVICE_ID);
	if (config == NULL) {
		xil_printf("No disparity VDMA found for ID %d. Update the Vitis platform from the new XSA.\r\n",
			DISPARITY_VDMA_DEVICE_ID);
		return XST_FAILURE;
	}

	status = XAxiVdma_CfgInitialize(disparity_vdma, config,
		config->BaseAddress);
	if (status != XST_SUCCESS)
		return status;

	read_cfg.EnableCircularBuf = 1;
	read_cfg.EnableFrameCounter = 0;
	read_cfg.FixedFrameStoreAddr = 0;
	read_cfg.EnableSync = 0;
	read_cfg.PointNum = 0;
	read_cfg.FrameDelay = 0;
	read_cfg.VertSizeInput = DISPARITY_HEIGHT;
	read_cfg.HoriSizeInput = DISPARITY_WIDTH * DISPARITY_BYTES_PER_PIXEL;
	read_cfg.Stride = DISPARITY_STRIDE;
	for (i = 0U; i < DISPARITY_FRAME_COUNT; i++)
		read_cfg.FrameStoreStartAddr[i] = frame_addr[i];

	status = XAxiVdma_DmaConfig(disparity_vdma, XAXIVDMA_READ, &read_cfg);
	if (status != XST_SUCCESS)
		return status;
	status = XAxiVdma_DmaSetBufferAddr(disparity_vdma, XAXIVDMA_READ,
		read_cfg.FrameStoreStartAddr);
	if (status != XST_SUCCESS)
		return status;
	status = XAxiVdma_DmaStart(disparity_vdma, XAXIVDMA_READ);
	if (status != XST_SUCCESS)
		return status;

	return XAxiVdma_StartParking(disparity_vdma, 0U, XAXIVDMA_READ);
}

#endif

static int disparity_vdma_park(XAxiVdma *disparity_vdma, u32 frame_index)
{
	u32 waited_us = 0U;
	int status = XAxiVdma_StartParking(disparity_vdma, frame_index,
		XAXIVDMA_READ);

	if (status != XST_SUCCESS)
		return status;
	while (XAxiVdma_CurrFrameStore(disparity_vdma, XAXIVDMA_READ) !=
			frame_index) {
		if (waited_us >= 100000U)
			return XST_FAILURE;
		usleep(100U);
		waited_us += 100U;
	}
	return XST_SUCCESS;
}

#if ROI_ARCHITECTURE
#include "roi_pipeline_capture.h"
#elif SGM2_ENABLED && !CORRECTED_BINOCULAR_DISPLAY
#if SGM2_FROZEN_OVERLAY
#include "frozen_overlay_capture.h"
#endif
#include "sgm2_capture.h"
#endif

static int app_failure(void)
{
#if ROI_ARCHITECTURE
 roi_alarm_off();
#endif
 return XST_FAILURE;
}

static int stereo_match_run(u32 frame_index, u32 *elapsed_us)
{
	XTime begin, now;
	u32 status;
	u32 started = 0U;

	StmWrite(STEREO_MATCH_BASEADDR, STM_FRAME_SEL,
		frame_index % DISPARITY_FRAME_COUNT);
	XTime_GetTime(&begin);
	StmWrite(STEREO_MATCH_BASEADDR, STM_ID, 1U);

	while (1) {
		status = StmRead(STEREO_MATCH_BASEADDR, STM_STATUS);
		if ((status & STM_STATUS_BUSY) || !(status & STM_STATUS_DONE))
			started = 1U;
		if (status & STM_STATUS_ERROR) {
			xil_printf("Stereo error: status=0x%08X code=0x%08X pixels=%d cycles=%d\r\n",
				status,
				StmRead(STEREO_MATCH_BASEADDR, STM_ERROR_CODE),
				StmRead(STEREO_MATCH_BASEADDR, STM_PIXEL_COUNT),
				StmRead(STEREO_MATCH_BASEADDR, STM_CYCLE_COUNT));
			return XST_FAILURE;
		}
		if (started && (status & STM_STATUS_DONE) &&
			!(status & STM_STATUS_BUSY)) {
			XTime_GetTime(&now);
			*elapsed_us = remap_elapsed_us(begin, now);
			if (StmRead(STEREO_MATCH_BASEADDR, STM_PIXEL_COUNT) !=
				DISPARITY_WIDTH * DISPARITY_HEIGHT) {
				xil_printf("Stereo pixel-count mismatch: %d\r\n",
					StmRead(STEREO_MATCH_BASEADDR, STM_PIXEL_COUNT));
				return XST_FAILURE;
			}
			return XST_SUCCESS;
		}

		XTime_GetTime(&now);
		if (remap_elapsed_us(begin, now) >= STEREO_TIMEOUT_US) {
			xil_printf("Stereo timeout: status=0x%08X pixels=%d cycles=%d error=0x%08X\r\n",
				status,
				StmRead(STEREO_MATCH_BASEADDR, STM_PIXEL_COUNT),
				StmRead(STEREO_MATCH_BASEADDR, STM_CYCLE_COUNT),
				StmRead(STEREO_MATCH_BASEADDR, STM_ERROR_CODE));
			return XST_FAILURE;
		}
		usleep(100U);
	}
}

/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */

#include "stereo_fixed_replay.h"

/* Drain previously prefetched frames while the matcher is unarmed. The
 * corrected buffers remain immutable throughout drain and matching. */
/* Stable XSCT breakpoint after map initialization; no board action by itself. */
__attribute__((noinline)) void rectify_maps_wait_marker(void)
{ __asm__ volatile ("" ::: "memory"); }

static int stereo_wait_fresh_input(void)
{
	u32 l = StmRead(STEREO_MATCH_BASEADDR, STM_SOF_L);
	u32 r = StmRead(STEREO_MATCH_BASEADDR, STM_SOF_R);
	XTime begin, now;
	XTime_GetTime(&begin);
	while (StmRead(STEREO_MATCH_BASEADDR, STM_SOF_L) - l < 3U ||
	       StmRead(STEREO_MATCH_BASEADDR, STM_SOF_R) - r < 3U) {
		XTime_GetTime(&now);
		if (remap_elapsed_us(begin, now) >= STEREO_TIMEOUT_US) {
			xil_printf("Continuous input SOF timeout\r\n");
			return XST_FAILURE;
		}
		usleep(100U);
	}
	return XST_SUCCESS;
}

static int application_run(void) {
    extern char _end;
    if ((UINTPTR)&_end > DISPARITY_FRAME0_ADDR) {
        xil_printf("STOP: application memory overlaps disparity reservation.\r\n");
        return XST_FAILURE;
    }
    if (rx_preflight() != XST_SUCCESS) return XST_FAILURE;
	xil_printf("--- Dual Camera Starting ---\r\n");
	xil_printf("STDOUT at 0x%08x\r\n", STDOUT_BASEADDRESS);

	int Status;
	XAxiVdma_Config *vdmaConfig;
	int i, j;
	XAxiVdma vmda0, vmda1;
	XAxiVdma disparity_vdma;
	const u32 disparity_frame_addr[DISPARITY_FRAME_COUNT] = {
		DISPARITY_FRAME0_ADDR,
		DISPARITY_FRAME1_ADDR,
		DISPARITY_FRAME2_ADDR
	};
	vdma[0] = &vmda0;
	vdma[1] = &vmda1;

	leftRectifyMap = (u32 *)&rectifyStorage[0][0];
	rightRectifyMap = (u32 *)(&rectifyStorage[0][0] + RECTIFY_MAP_BYTES);
	for (i = 0; i < RECTIFY_OUTPUT_FRAMES; i++) {
		leftCorrectedFrame[i] = &rectifyStorage[1U + i][0];
		rightCorrectedFrame[i] =
			&rectifyStorage[1U + RECTIFY_OUTPUT_FRAMES + i][0];
	}

	/*
	 * Mark both map regions as empty.  XSCT later replaces these bytes with
	 * the real tables; the capture flow can then detect them automatically.
	 */
	memset(leftRectifyMap, 0xFF, RECTIFY_MAP_BYTES);
	memset(rightRectifyMap, 0xFF, RECTIFY_MAP_BYTES);
	Xil_DCacheFlushRange((INTPTR)leftRectifyMap, RECTIFY_MAP_BYTES);
	Xil_DCacheFlushRange((INTPTR)rightRectifyMap, RECTIFY_MAP_BYTES);

	xil_printf("\r\n=== Rectification DDR layout ===\r\n");
	xil_printf("Left map addr        = 0x%08X\r\n", (u32)leftRectifyMap);
	xil_printf("Right map addr       = 0x%08X\r\n", (u32)rightRectifyMap);
	for (i = 0; i < RECTIFY_OUTPUT_FRAMES; i++) {
		xil_printf("Left corrected[%d]  = 0x%08X\r\n", i,
			(u32)leftCorrectedFrame[i]);
		xil_printf("Right corrected[%d] = 0x%08X\r\n", i,
			(u32)rightCorrectedFrame[i]);
	}
	xil_printf("Map bytes each       = %d\r\n", RECTIFY_MAP_BYTES);
	xil_printf("Corrected bytes each = %d\r\n", RECTIFY_OUTPUT_BYTES);
	xil_printf("Storage region bytes = %d\r\n", DEMO_MAX_FRAME);
	xil_printf("Storage end          = 0x%08X\r\n",
			(u32)((u8 *)rectifyStorage + sizeof(rectifyStorage)));
	xil_printf("\r\n=== Stereo disparity DDR layout ===\r\n");
	for (i = 0; i < DISPARITY_FRAME_COUNT; i++) {
		memset((void *)disparity_frame_addr[i], 0, DISPARITY_FRAME_BYTES);
		Xil_DCacheFlushRange((INTPTR)disparity_frame_addr[i],
			DISPARITY_FRAME_BYTES);
		xil_printf("Disparity[%d]         = 0x%08X - 0x%08X\r\n",
			i, disparity_frame_addr[i],
			disparity_frame_addr[i] + DISPARITY_FRAME_BYTES - 1U);
	}
	xil_printf("Disparity bytes each = %d, stride=%d\r\n",
		DISPARITY_FRAME_BYTES, DISPARITY_STRIDE);

	/* Cameras are initialized on FPGA1; MS7210 is configured by PL. */

	/*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_VDMA; i++) {
		for (j = 0; j < DISPLAY_NUM_FRAMES; j++) {
			pFrames[i][j] = frameBuf[i][j];
			memset(pFrames[i][j], 0, DEMO_MAX_FRAME);
			Xil_DCacheFlushRange((INTPTR) pFrames[i][j], DEMO_MAX_FRAME) ;
		}
	}
	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig) {
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
        return XST_FAILURE;
	}

	Status = XAxiVdma_CfgInitialize(vdma[0], vdmaConfig,
			vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
        return XST_FAILURE;
	}

	vdmaConfig = XAxiVdma_LookupConfig(XPAR_AXI_VDMA_3_DEVICE_ID);
	if (!vdmaConfig) {
		xil_printf("No video DMA found for ID %d\r\n",
				XPAR_AXI_VDMA_3_DEVICE_ID);
        return XST_FAILURE;
	}
	Status = XAxiVdma_CfgInitialize(vdma[1], vdmaConfig,
			vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);
        return XST_FAILURE;
	}






//	xil_printf("\r\n");
//	xil_printf("========== FRAME BUFFER CHECK ==========\r\n");
//
//	xil_printf("curFrame[0] = %d\r\n", dispCtrl.curFrame[0]);
//	xil_printf("curFrame[1] = %d\r\n", dispCtrl.curFrame[1]);
//
//	xil_printf("\r\nCAM0 Frame Buffers:\r\n");
//	xil_printf("framePtr[0][0] = 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[0][0]);
//	xil_printf("framePtr[0][1] = 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[0][1]);
//	xil_printf("framePtr[0][2] = 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[0][2]);
//
//	xil_printf("\r\nCAM1 Frame Buffers:\r\n");
//	xil_printf("framePtr[1][0] = 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[1][0]);
//	xil_printf("framePtr[1][1] = 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[1][1]);
//	xil_printf("framePtr[1][2] = 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[1][2]);
//
//	xil_printf("\r\nCurrent Write Address:\r\n");
//	xil_printf("CAM0 -> 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[0][dispCtrl.curFrame[0]]);
//	xil_printf("CAM1 -> 0x%08X\r\n",
//	        (u32)dispCtrl.framePtr[1][dispCtrl.curFrame[1]]);
//
//	xil_printf("========================================\r\n");







	stride[0] = 1920 * 4;
	stride[1] = 1920 * 4;
	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, vdma, DISP_VTC_ID, DYNCLK_BASEADDR,
			pFrames, stride);
	if (Status != XST_SUCCESS) {
		xil_printf(
				"Display Ctrl initialization failed during demo initialization%d\r\n",
				Status);
        return XST_FAILURE;
	}
	dispCtrl.HoriSizeInput[0] = 1280 * 4;
	dispCtrl.VertSizeInput[0] = 720;
	dispCtrl.HoriSizeInput[1] = 1280 * 4;
	dispCtrl.VertSizeInput[1] = 720;
	#if SGM2_ENABLED && !CORRECTED_BINOCULAR_DISPLAY
    Status = DisplayStartTimingOnly(&dispCtrl);
#else
	Status = DisplayStart(&dispCtrl);
#endif
	if (Status != XST_SUCCESS) {
		xil_printf("Couldn't start display during demo initialization%d\r\n",
				Status);
        return XST_FAILURE;
	}




	xil_printf("\r\n===== AFTER INIT =====\r\n");

	for(int i=0;i<2;i++)
	{
	    for(int j=0;j<3;j++)
	    {
	        xil_printf(
	            "dispCtrl.framePtr[%d][%d]=0x%08X\r\n",
	            i,j,
	            (u32)dispCtrl.framePtr[i][j]
	        );
	    }
	}

	for(int i=0;i<2;i++)
	{
	    for(int j=0;j<3;j++)
	    {
	        xil_printf("pFrames[%d][%d]=0x%08X\r\n",
	            i,j,(u32)pFrames[i][j]);
	    }
	}





#if SGM2_ENABLED && !CORRECTED_BINOCULAR_DISPLAY && SGM2_FIXED_REPLAY
    Status = sgm2_replay(&dispCtrl, &disparity_vdma,
        leftCorrectedFrame[0], rightCorrectedFrame[0], disparity_frame_addr[0]);
    xil_printf("SGM2 replay finished status=%d; reconfigure PL to rerun.\r\n",Status);
    while(1) usleep(100000);
#endif
#if STEREO_FIXED_REPLAY && !CORRECTED_BINOCULAR_DISPLAY && !SGM2_ENABLED
	Status = stereo_fixed_replay(&dispCtrl, &disparity_vdma,
		leftCorrectedFrame[0], rightCorrectedFrame[0], disparity_frame_addr);
	xil_printf("Fixed replay stopped: status=%d. Reset to rerun.\r\n", Status);
	while (1) usleep(100000);
#endif
	Status = vdma_write_init(&cam_vdma0, XPAR_AXI_VDMA_0_DEVICE_ID,
			1280 * 4, 720, 1920 * 4,
			(unsigned int)dispCtrl.framePtr[0][0]);
	if (Status != XST_SUCCESS) {
		xil_printf("Camera 0 S2MM start failed: %d\r\n", Status);
		return XST_FAILURE;
	}

	Status = vdma_write_init(&cam_vdma1, XPAR_AXI_VDMA_1_DEVICE_ID,
			1280 * 4, 720, 1920 * 4,
			(unsigned int)dispCtrl.framePtr[1][0]);
	if (Status != XST_SUCCESS) {
		xil_printf("Camera 1 S2MM start failed: %d\r\n", Status);
		return XST_FAILURE;
	}

	/* Cameras now remain in three-frame circular S2MM mode. */
	{
		XAxiVdma *camera_vdma[DISPLAY_NUM_VDMA] = {&cam_vdma0, &cam_vdma1};
		u32 last_current[DISPLAY_NUM_VDMA];
		u32 current_frame[DISPLAY_NUM_VDMA];
		u32 completed_frame[DISPLAY_NUM_VDMA];
		u32 completed_addr[DISPLAY_NUM_VDMA];
		u32 output_index = 0U;
		u32 displayed_pairs = 0U;

		xil_printf("\r\n=== Waiting for rectification maps ===\r\n");
		xil_printf("Load LEFT  map (%d bytes) to 0x%08X\r\n",
			RECTIFY_MAP_BYTES, (u32)leftRectifyMap);
		xil_printf("Load RIGHT map (%d bytes) to 0x%08X\r\n",
			RECTIFY_MAP_BYTES, (u32)rightRectifyMap);
		xil_printf("The program will continue automatically after both maps are detected.\r\n");

		/*
		 * The centre destination pixel is valid in both generated maps.  Both
		 * regions were pre-filled with 0xFFFFFFFF, so a changed centre word is
		 * a simple ready flag that needs no UART interaction.  XSCT may stop the
		 * CPU while downloading; after 'con', this loop resumes safely.
		 */
		{
			const u32 probe_index =
				(RECTIFY_MAP_HEIGHT / 2U) * RECTIFY_MAP_WIDTH +
				(RECTIFY_MAP_WIDTH / 2U);
			u32 left_ready = 0U;
			u32 right_ready = 0U;

			rectify_maps_wait_marker();
			while (!(left_ready && right_ready)) {
				Xil_DCacheInvalidateRange(
					(INTPTR)&leftRectifyMap[probe_index], sizeof(u32));
				Xil_DCacheInvalidateRange(
					(INTPTR)&rightRectifyMap[probe_index], sizeof(u32));
				left_ready =
					(leftRectifyMap[probe_index] != 0xFFFFFFFFU);
				right_ready =
					(rightRectifyMap[probe_index] != 0xFFFFFFFFU);
				usleep(10000);
			}
		}

		xil_printf("Both rectification maps detected automatically.\r\n");

		/* XSCT writes DDR behind the CPU cache, so discard stale map lines. */
		Xil_DCacheInvalidateRange((INTPTR)leftRectifyMap, RECTIFY_MAP_BYTES);
		Xil_DCacheInvalidateRange((INTPTR)rightRectifyMap, RECTIFY_MAP_BYTES);

		xil_printf("\r\n=== Programming tile descriptors into IP1 BRAM ===\r\n");
		Status = program_tile_descriptors(leftRectifyMap,
			REMAP_GENERATE_LEFT_BASEADDR, LEFT_DESCRIPTOR_BASE, "LEFT");
		if (Status != XST_SUCCESS)
			return XST_FAILURE;
		Status = program_tile_descriptors(rightRectifyMap,
			REMAP_GENERATE_RIGHT_BASEADDR, RIGHT_DESCRIPTOR_BASE, "RIGHT");
		if (Status != XST_SUCCESS)
			return XST_FAILURE;

		xil_printf("\r\n=== Continuous stereo rectification ===\r\n");
		xil_printf("LEFT  IP1=0x%08X IP2=0x%08X\r\n",
			REMAP_GENERATE_LEFT_BASEADDR, REMAP_ACCEL_LEFT_BASEADDR);
		xil_printf("RIGHT IP1=0x%08X IP2=0x%08X\r\n",
			REMAP_GENERATE_RIGHT_BASEADDR, REMAP_ACCEL_RIGHT_BASEADDR);
		xil_printf("Camera S2MM and rectification are running continuously.\r\n");

        if (CORRECTED_BINOCULAR_DISPLAY) {
            axis_switch_select_binocular_path();
            xil_printf("\r\n=== Corrected binocular display selected ===\r\n");
            xil_printf("Corrected LEFT/RIGHT -> VDMA2/3 -> OSD -> HDMI\r\n");
            xil_printf("Display switch = 0x%08X (S00 binocular selected)\r\n",
                DISPLAY_SWITCH_BASEADDR);
        } else {
#if ROI_ARCHITECTURE
        if(roi_preflight()!=XST_SUCCESS)return XST_FAILURE;
#elif SGM2_ENABLED && !CORRECTED_BINOCULAR_DISPLAY
        if(sgm2_preflight(&dispCtrl)!=XST_SUCCESS) return XST_FAILURE;
        xil_printf("SGM2: capture first corrected pair, then single match and freeze.\r\n");
#else
		if (StmRead(STEREO_MATCH_BASEADDR, STM_ID) != STM_CORE_ID ||
		    StmRead(STEREO_MATCH_BASEADDR, STM_BUILD) != 0x20260902U) {
			xil_printf("Stereo matcher ID error at 0x%08X: read 0x%08X\r\n",
				STEREO_MATCH_BASEADDR,
				StmRead(STEREO_MATCH_BASEADDR, STM_ID));
			return XST_FAILURE;
		}
		StmWrite(STEREO_MATCH_BASEADDR, STM_DEST0,
			disparity_frame_addr[0]);
		StmWrite(STEREO_MATCH_BASEADDR, STM_DEST1,
			disparity_frame_addr[1]);
		StmWrite(STEREO_MATCH_BASEADDR, STM_DEST2,
			disparity_frame_addr[2]);
		StmWrite(STEREO_MATCH_BASEADDR, STM_WIDTH, DISPARITY_WIDTH);
		StmWrite(STEREO_MATCH_BASEADDR, STM_HEIGHT, DISPARITY_HEIGHT);
		StmWrite(STEREO_MATCH_BASEADDR, STM_STRIDE, DISPARITY_STRIDE);
		StmWrite(STEREO_MATCH_BASEADDR, STM_UNIQUENESS, 2U);

		Status = disparity_vdma_initialize(&disparity_vdma,
			disparity_frame_addr);
		if (Status != XST_SUCCESS) {
			xil_printf("Disparity MM2S VDMA initialization failed: %d\r\n",
				Status);
			return XST_FAILURE;
		}

		axis_switch_select_stereo_path();
		xil_printf("\r\n=== Stereo validation path selected ===\r\n");
		xil_printf("Corrected LEFT/RIGHT -> Stereo matcher -> DDR disparity -> VDMA4 -> HDMI\r\n");
		xil_printf("Input switches  = 0x%08X, 0x%08X\r\n",
			LEFT_INPUT_SWITCH_BASEADDR, RIGHT_INPUT_SWITCH_BASEADDR);
		xil_printf("Display switch = 0x%08X (S01 disparity selected)\r\n",
			DISPLAY_SWITCH_BASEADDR);
#endif

        }

		if (rx_enable() != XST_SUCCESS) return XST_FAILURE;
		last_current[0] = XAxiVdma_CurrFrameStore(camera_vdma[0],
			XAXIVDMA_WRITE);
		last_current[1] = XAxiVdma_CurrFrameStore(camera_vdma[1],
			XAXIVDMA_WRITE);

		while (1) {
			u32 left_us, right_us, pair_us;
			XTime loop_begin, loop_end;
			u32 stereo_us = 0U;
#if SGM2_CONTINUOUS && ROI_ARCHITECTURE
            u32 capture_park_us=0,capture_pre_us=0;
#endif
			u32 left_advanced = 0U, right_advanced = 0U;
			u32 display_slot;
			XTime_GetTime(&loop_begin);

#if SGM2_CONTINUOUS
            stream_pause_boundary();
            overlay_request[14]=1;Xil_DCacheFlushRange((INTPTR)overlay_request,sizeof(overlay_request));
#endif
#if !(SGM2_CONTINUOUS && ROI_ARCHITECTURE)
			/* Wait until each camera has completed at least one new frame. */
			while (!(left_advanced && right_advanced)) {
                XTime capture_now;
                if (rx_check_capture(camera_vdma[0], camera_vdma[1]) != XST_SUCCESS)
                    return XST_FAILURE;
                XTime_GetTime(&capture_now);
                if (remap_elapsed_us(loop_begin, capture_now) > 60000000U) {
                    xil_printf("RX frame timeout: left_slot=%d right_slot=%d status=%08X\r\n",
                        XAxiVdma_CurrFrameStore(camera_vdma[0],XAXIVDMA_WRITE),
                        XAxiVdma_CurrFrameStore(camera_vdma[1],XAXIVDMA_WRITE),rx_status());
                    rx_disable();
                    XAxiVdma_DmaStop(camera_vdma[0],XAXIVDMA_WRITE);
                    XAxiVdma_DmaStop(camera_vdma[1],XAXIVDMA_WRITE);
                    return XST_FAILURE;
                }

				current_frame[0] = XAxiVdma_CurrFrameStore(
					camera_vdma[0], XAXIVDMA_WRITE);
				current_frame[1] = XAxiVdma_CurrFrameStore(
					camera_vdma[1], XAXIVDMA_WRITE);
				left_advanced = (current_frame[0] != last_current[0]);
				right_advanced = (current_frame[1] != last_current[1]);
				if (!(left_advanced && right_advanced)) usleep(100U);
			}

			/* The slot immediately before the current S2MM slot is complete. */
			completed_frame[0] = (current_frame[0] +
				DISPLAY_NUM_FRAMES - 1U) % DISPLAY_NUM_FRAMES;
			completed_frame[1] = (current_frame[1] +
				DISPLAY_NUM_FRAMES - 1U) % DISPLAY_NUM_FRAMES;
			completed_addr[0] = (u32)pFrames[0][completed_frame[0]];
			completed_addr[1] = (u32)pFrames[1][completed_frame[1]];
			last_current[0] = current_frame[0];
			last_current[1] = current_frame[1];

#endif

#if SGM2_CONTINUOUS && ROI_ARCHITECTURE
            if(stream_capture_hold(camera_vdma,completed_frame,&capture_park_us)!=XST_SUCCESS)
                return XST_FAILURE;
            completed_addr[0]=(u32)pFrames[0][completed_frame[0]];
            completed_addr[1]=(u32)pFrames[1][completed_frame[1]];
#elif SGM2_CONTINUOUS
            {static unsigned drops;
            if(stream_snapshot(camera_vdma,current_frame,completed_addr)) {
                if(++drops%30==1)xil_printf("STREAM snapshot retry count=%d; writer advanced during copy.\r\n",drops);
                if(drops>=300){xil_printf("STREAM STOP: snapshot cannot keep up.\r\n");return XST_FAILURE;}
                continue;
            }
            drops=0;completed_addr[1]=STREAM_RAW_LEFT;completed_addr[0]=STREAM_RAW_RIGHT;}
#endif
			/* Calibration camera identities differ from physical RX lane names.
			 * RX1 -> LEFT map/output; RX0 -> RIGHT map/output.
			 * Verified by software rectification of the 2026-09-17 capture. */
			Status = remap_stereo_run(completed_addr[1], completed_addr[0],
				(u32)leftCorrectedFrame[output_index],
				(u32)rightCorrectedFrame[output_index],
				&left_us, &right_us, &pair_us);
#if SGM2_CONTINUOUS && ROI_ARCHITECTURE
            {
                int protected_ok=stream_capture_protected();
                stream_capture_release();
                if(Status!=XST_SUCCESS || protected_ok!=XST_SUCCESS){
                    xil_printf("CAPTURE STOP: rectification failed or raw protection lost.\r\n");
                    return XST_FAILURE;
                }
            }
#endif
			if (Status != XST_SUCCESS) return XST_FAILURE;

			if (rx_check(camera_vdma[0], camera_vdma[1]) != XST_SUCCESS) return XST_FAILURE;
#if ROI_ARCHITECTURE
#if SGM2_CONTINUOUS
            XTime_GetTime(&loop_end);capture_pre_us=remap_elapsed_us(loop_begin,loop_end);
#endif
            Status=roi_dual_capture_once(leftCorrectedFrame[output_index],rightCorrectedFrame[output_index]);
            if(Status!=XST_SUCCESS){xil_printf("ROI STOP: processing failed; previous HDMI frame retained.\r\n");return XST_FAILURE;}
#if SGM2_CONTINUOUS
            XTime_GetTime(&loop_end);
            xil_printf("PERF CAPTURE direct=1 copy_us=0 park_wait=%d rect_left=%d rect_right=%d rect_pair=%d pre_roi=%d round=%d raw_RX0=%d raw_RX1=%d units=us\r\n",
                capture_park_us,left_us,right_us,pair_us,capture_pre_us,
                remap_elapsed_us(loop_begin,loop_end),completed_frame[0],completed_frame[1]);
#endif
            displayed_pairs++;output_index=(output_index+1U)%RECTIFY_OUTPUT_FRAMES;
            continue;
#elif SGM2_ENABLED && !CORRECTED_BINOCULAR_DISPLAY
            Status = sgm2_capture_once(&dispCtrl,&disparity_vdma,
                leftCorrectedFrame[output_index],rightCorrectedFrame[output_index],
                disparity_frame_addr[0]);
#if SGM2_CONTINUOUS
            if(Status!=XST_SUCCESS){xil_printf("STREAM STOP: frame processing failed; previous HDMI frame retained.\r\n");return XST_FAILURE;}
            displayed_pairs++;
            XTime_GetTime(&loop_end);
            xil_printf("STREAM frame=%d slot=%d time_us=%d depth_status=%d pause_command=%08X\r\n",
                displayed_pairs,output_index,remap_elapsed_us(loop_begin,loop_end),overlay_depth_result[2],(u32)(UINTPTR)stream_command);
            output_index=(output_index+1U)%RECTIFY_OUTPUT_FRAMES;
            continue;
#else
            xil_printf("SGM2 single capture finished status=%d; reconfigure PL to rerun.\r\n",Status);
            while(1) usleep(100000);
#endif
#endif
			/* Feed the corrected pair to the selected display/matcher path. */
			display_slot = (output_index + 1U) % DISPLAY_NUM_FRAMES;
			display_set_corrected_slot(&dispCtrl, display_slot,
				leftCorrectedFrame[output_index],
				rightCorrectedFrame[output_index]);
			Status = display_park_corrected_pair(&dispCtrl, display_slot);
			if (Status != XST_SUCCESS) {
				xil_printf("Corrected display switch failed: %d\r\n", Status);
				return XST_FAILURE;
			}
			Status = display_wait_for_pair(&dispCtrl, display_slot);
			if (Status != XST_SUCCESS) return XST_FAILURE;
            if (!CORRECTED_BINOCULAR_DISPLAY) {
			Status = stereo_wait_fresh_input();
			if (Status != XST_SUCCESS) return XST_FAILURE;

			Status = stereo_match_run(output_index, &stereo_us);
			if (Status != XST_SUCCESS) return XST_FAILURE;

			/* The matcher has finished writing this DDR slot.  Only now let
			 * VDMA4 display it, while the next slot is being calculated. */
			if (rx_check(camera_vdma[0], camera_vdma[1]) != XST_SUCCESS) return XST_FAILURE;
			Status = disparity_vdma_park(&disparity_vdma, output_index);
			if (Status != XST_SUCCESS) {
				xil_printf("Disparity display park failed: frame=%d status=%d\r\n",
					output_index, Status);
				return XST_FAILURE;
			}

            }

			displayed_pairs++;
			XTime_GetTime(&loop_end);
			if (displayed_pairs <= 5U || (displayed_pairs % 30U) == 0U) {
                if (CORRECTED_BINOCULAR_DISPLAY) {
                    xil_printf("Binocular pair %d raw=(%d,%d) corrected_slot=%d remap=(%d,%d,%d) us loop=%d us\r\n",
                        displayed_pairs, completed_frame[0], completed_frame[1],
                        display_slot, left_us, right_us, pair_us,
                        remap_elapsed_us(loop_begin, loop_end));
                } else {
				xil_printf("Pair %d raw=(%d,%d) corrected=%d disparity=%d remap=(%d,%d,%d) us stereo=%d us cycles=%d\r\n",
					displayed_pairs, completed_frame[0], completed_frame[1],
					display_slot, output_index, left_us, right_us, pair_us,
					stereo_us,
					StmRead(STEREO_MATCH_BASEADDR, STM_CYCLE_COUNT));
				xil_printf("Full loop=%d us (capture/remap/input drain/match/display)\r\n",
					remap_elapsed_us(loop_begin, loop_end));
                }
			}
			output_index = (output_index + 1U) % RECTIFY_OUTPUT_FRAMES;
		}
	}

#if 0 /* Legacy DDR diagnostics modify active frame buffers; keep disabled. */




		u32 *p = (u32*)dispCtrl.framePtr[0][0];

		Xil_DCacheInvalidateRange((INTPTR)p, 128);

		for(int i=0;i<32;i++)
		{
		    xil_printf("%08X\r\n", p[i]);
		}



//		u32 *p0 = (u32 *)dispCtrl.framePtr[0][dispCtrl.curFrame[0]];
//
//		Xil_DCacheInvalidateRange(
//		    (INTPTR)p0,
//		    64);
//
//		for(int i=0;i<16;i++)
//		{
//		    xil_printf("[%02d] %08X\r\n", i, p0[i]);
//		}











	// ===== 璇婃柇锛氭鏌ュ抚缂撳啿鍦板潃鏄惁鍦  DDR 鍖哄煙鍐 ?=====
	xil_printf("\r\n=== DDR Address Check ===\r\n");
	xil_printf("DDR range: 0x%08x - 0x%08x\r\n", 0x00100000, 0x00100000 + 0x3FF00000);
	for (i = 0; i < DISPLAY_NUM_VDMA; i++) {
		for (j = 0; j < DISPLAY_NUM_FRAMES; j++) {
			u32 addr = (u32)dispCtrl.framePtr[i][j];
			int in_ddr = (addr >= 0x00100000 && addr < 0x00100000 + 0x3FF00000);
			xil_printf("  framePtr[%d][%d] = 0x%08x  %s DDR\r\n",
					i, j, addr, in_ddr ? "鉁  IN" : "鉁  OUTSIDE");
		}
	}

	// ===== 璇婃柇锛氭鏌  VDMA 鐘舵 佸苟璇诲彇 DDR 鍓  30 瀛楄 ?=====
	xil_printf("\r\n=== VDMA Status ===\r\n");
	{
		XAxiVdma *vlist[2] = {&cam_vdma0, &cam_vdma1};
		const char *vname[2] = {"Camera0", "Camera1"};
		int vi;
		for (vi = 0; vi < 2; vi++) {
			u32 ver = XAxiVdma_GetVersion(vlist[vi]);
			u32 busy = XAxiVdma_IsBusy(vlist[vi], XAXIVDMA_WRITE);
			u32 err  = XAxiVdma_GetDmaChannelErrors(vlist[vi], XAXIVDMA_WRITE);
			xil_printf("  %s: Ver=0x%08x Busy=%d Err=0x%08x\r\n",
					vname[vi], ver, busy, err);
		}
	}

	// ===== 璇诲 ?Frame Buffer 鍓  30 瀛楄 ?=====
	xil_printf("\r\n=== Frame Buffer Data (hex, first 30 bytes) ===\r\n");
	for (i = 0; i < DISPLAY_NUM_VDMA; i++) {
			Xil_DCacheInvalidateRange((INTPTR)dispCtrl.framePtr[i][0], 64);
		volatile u8 *buf = (volatile u8 *)dispCtrl.framePtr[i][0];
		xil_printf("  Pipe[%d] @0x%08x: ", i, (u32)buf);
		int k;
		for (k = 0; k < 30; k++) {
			xil_printf("%02x ", buf[k]);
		}
		xil_printf("\r\n");
	}

		// ===== Robust DDR Read Test =====
		{
			volatile u8 *p = (volatile u8 *)dispCtrl.framePtr[0][0];
			Xil_DCacheFlushRange((INTPTR)p, 64);
			Xil_DCacheInvalidateRange((INTPTR)p, 64);
			xil_printf("\r\n=== DDR Read Test @0x%08x ===\r\n", (u32)p);
			xil_printf("  [0..15]: ");
			for (int ii = 0; ii < 16; ii++) xil_printf("%02x ", p[ii]);
			xil_printf("\r\n");
			xil_printf("  [64..79]: ");
			for (int ii = 64; ii < 80; ii++) xil_printf("%02x ", p[ii]);
			xil_printf("\r\n");
		}

		// ===== VDMA Write Test: write 0xa5 then check =====
		xil_printf("\r\n=== VDMA Write Test ===\r\n");
		{
			volatile u8 *wp = (volatile u8 *)dispCtrl.framePtr[0][0];
			for (int jj = 0; jj < 64; jj++) wp[jj] = 0xa5;
			Xil_DCacheFlushRange((INTPTR)wp, 128);
		}
		usleep(500000);
		{
			volatile u8 *rp = (volatile u8 *)dispCtrl.framePtr[0][0];
			Xil_DCacheFlushRange((INTPTR)rp, 128);
			Xil_DCacheInvalidateRange((INTPTR)rp, 128);
			xil_printf("  After 0.5s [0..7] = ");
			int allA5 = 1;
			for (int jj = 0; jj < 8; jj++) {
				xil_printf("%02x ", rp[jj]);
				if (rp[jj] != 0xa5) allA5 = 0;
			}
			xil_printf("  %s\r\n", allA5 ? "(all 0xa5=VDMA NOT writing!)" : "(overwritten=VDMA IS writing!)");
		}

	return 0;
#endif
}

/* Any error return closes the receiver; no partial stream is left running. */
int main(void)
{
    int result=application_run();
#if SGM2_ENABLED && !CORRECTED_BINOCULAR_DISPLAY && SGM2_FROZEN_OVERLAY && !ROI_ARCHITECTURE
    xyz_state(0,XYZ_PIPELINE_STOP);
#endif
    rx_disable();
    xil_printf("Application stopped: status=%d. Reset and reload maps to retry.\r\n", result);
    return result;
}
