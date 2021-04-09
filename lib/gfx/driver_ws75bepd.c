/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

#include "gfx.h"

#if GFX_USE_GDISP

#define GDISP_DRIVER_VMT			GDISPVMT_WS75bEPD
#include "gdisp_lld_config.h"
#include "ugfx/src/gdisp/gdisp_driver.h"

#include "board_WS75bEPD.h"
#include "WS75bEPD.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#ifndef GDISP_SCREEN_HEIGHT
	#define GDISP_SCREEN_HEIGHT		384
#endif
#ifndef GDISP_SCREEN_WIDTH
	#define GDISP_SCREEN_WIDTH		640
#endif

gU8 powerSettingData[] = {0x37, 0x00};
gU8 panelSettingData[] = {0xcf, 0x08};
gU8 boosterSoftStartData[] = {0xc7, 0xcc, 0x28};
gU8 pllControlData[] = {0x3c};
gU8 temperateCalibrationData[] = {0x00};
gU8 vcomAndDataIntervalData[] = {0x77};
gU8 tconSettingData[] = {0x22};
gU8 tconResolutionData[] = {0x02, 0x80, 0x01, 0x80};
gU8 vcmDcSettingData[] = {0x1e};
gU8 flashModeData[] = {0x03};

/* dithering theshold matrix */
gU8 thresholdMatrix[] = {0, 7, 3, 6, 5, 2, 4, 1, 8};
float thresholdDivisor = 9;


/* Every data byte determines 4 pixels. */
#ifndef WS75bEPD_PPB
  #define WS75bEPD_PPB   4
#endif

/*===========================================================================*/
/* Driver local variables.                                                   */
/*===========================================================================*/

/* initialization variables according to WaveShare. */
// gU8 LUTDefault_full[]    = {0x02,0x02,0x01,0x11,0x12,0x12,0x22,0x22,0x66,0x69,0x69,0x59,0x58,0x99,0x99,0x88,0x00,0x00,0x00,0x00,0xF8,0xB4,0x13,0x51,0x35,0x51,0x51,0x19,0x01,0x00}; // Initialize the full display

# define BLOCK_SIZE(x) sizeof(x) / sizeof(x[0])
/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static inline void startUpDisplay(GDisplay* g) {
	write_reg_data(g, POWER_SETTING, powerSettingData, BLOCK_SIZE(powerSettingData));
	write_reg_data(g, PANEL_SETTING, panelSettingData, BLOCK_SIZE(panelSettingData));
	write_reg_data(g, BOOSTER_SOFT_START, boosterSoftStartData, BLOCK_SIZE(boosterSoftStartData));
	write_cmd(g, POWER_ON);
	wait_until_idle(g);

	write_reg_data(g, PLL_CONTROL, pllControlData, BLOCK_SIZE(pllControlData));
	write_reg_data(g, TEMP_SENSOR_CTRL, temperateCalibrationData, BLOCK_SIZE(temperateCalibrationData));
	write_reg_data(g, VCOM_AND_DATA_INTERVAL_SETTING, vcomAndDataIntervalData, BLOCK_SIZE(vcomAndDataIntervalData));
	write_reg_data(g, TCON_SETTING, tconSettingData, BLOCK_SIZE(tconSettingData));
	write_reg_data(g, TCON_RESOLUTION, tconResolutionData, BLOCK_SIZE(tconResolutionData));
	write_reg_data(g, VCM_DC_SETTING, vcmDcSettingData, BLOCK_SIZE(vcmDcSettingData));
	write_reg_data(g, FLASH_MODE, flashModeData, BLOCK_SIZE(flashModeData));

	write_cmd(g, DATA_START_TRANSMISSION_1);
	gfxSleepMilliseconds(2);
}

inline gU8 orderedDithering(GDisplay* g, gCoord x, gCoord y) {
	gColor luma = EXACT_LUMA_OF(g->p.color);
	float thresholdingFactor = (float)(thresholdMatrix[y % 3 * 3 + x % 3]) / thresholdDivisor - 0.5;

	luma = luma + (int)(128.0 * thresholdingFactor);

	gU8 colorValue;
	if (luma < 128) {
		colorValue = PIXEL_COLOR_BLACK;
	} else {
		colorValue = PIXEL_COLOR_WHITE;
	}
	return colorValue;
}

static inline void resetDisplay(GDisplay* g) {
	setpin_reset(g, gFalse);
	gfxSleepMilliseconds(200);
	setpin_reset(g, gTrue);
	gfxSleepMilliseconds(200);
}

LLDSPEC gBool gdisp_lld_init(GDisplay *g) {
	/* Use the private area as a frame buffer.
	*
	* The frame buffer will be one big array of bytes storing all the pixels with WS29EPD_PPB pixel per byte.
	* For the layout, the frame will be stored line by line in the x-direction.
	* So: [Line x=0][Line x=1][Line x=2] ... [Line x=GDISP_SCREEN_WIDTH/WS29EPD_PPB]
	* And every x-line contains GDISP_SCREEN_HEIGHT y-values:
	* [y=0; y=1; y=2; y=3; ...; y=GDISP_SCREEN_HEIGHT][y=0; y=1; y=2; y=3; ...; y=GDISP_SCREEN_HEIGHT]...
	*
	*/

	g->priv = gfxAlloc((GDISP_SCREEN_WIDTH / WS75bEPD_PPB) * GDISP_SCREEN_HEIGHT * sizeof(gU8));
	if (!g->priv)
		return gFalse;

	/* Initialize the LL hardware. */
	init_board(g);

	/* Hardware reset. */
	resetDisplay(g);

	/* Acquire the bus for the whole initialization. */
	acquire_bus(g);

	/* Send the initialization commands. (according to WaveShare) */
	startUpDisplay(g);

	release_bus(g);

	/* Finish board initialization. */
	post_init_board(g);

	/* Initialise the GDISP structure */
	g->g.Width = GDISP_SCREEN_WIDTH;
	g->g.Height = GDISP_SCREEN_HEIGHT;
	g->g.Orientation = gOrientation0;
	g->g.Powermode = gPowerOn;
	return gTrue;
}

#if GDISP_HARDWARE_DRAWPIXEL
LLDSPEC void gdisp_lld_draw_pixel(GDisplay *g) {
	gCoord		x, y;

	switch(g->g.Orientation) {
	default:
	case gOrientation0:
		x = g->p.x;
		y = g->p.y;
		break;
	case gOrientation90:
		x = g->p.y;
		y = GDISP_SCREEN_HEIGHT - 1 - g->p.x;
		break;
	case gOrientation180:
		x = GDISP_SCREEN_WIDTH - 1 - g->p.x;
		y = GDISP_SCREEN_HEIGHT - 1 - g->p.y;
		break;
	case gOrientation270:
		x = GDISP_SCREEN_HEIGHT - 1 - g->p.y;
		y = g->p.x;
		break;
	}

	// 1. Delete old color (delete high or low bits depending on position)
	gU8 bitmask;
	gU8 shift;
	switch (x % 4) {
		case 0:
			bitmask = 252;  // 1111 1100
			shift = 0;
			break;
		case 1:
			bitmask = 243;  // 1111 0011
			shift = 2;
			break;
		case 2:
			bitmask = 207;  // 1100 1111
			shift = 4;
			break;
		default:
			bitmask = 63;   // 0011 1111
			shift = 6;
			break;
	}
	((gU8 *)g->priv)[(GDISP_SCREEN_HEIGHT*(x/WS75bEPD_PPB)) + y] &= bitmask;
	
	// 2. set new color
	gU8 colorValue;
	switch (g->p.color) {
		case GFX_WHITE:
			colorValue = PIXEL_COLOR_WHITE;
			break;
		case GFX_BLACK:
			colorValue = PIXEL_COLOR_BLACK;
			break;
		case GFX_RED:
			colorValue = PIXEL_COLOR_RED;
			break;
		default:
			colorValue = orderedDithering(g, x, y);
	}	
	((gU8 *)g->priv)[(GDISP_SCREEN_HEIGHT*(x/WS75bEPD_PPB)) + y] |= colorValue << shift;
}
#endif

#if GDISP_HARDWARE_FLUSH
static inline gU8 convertPixel(gU8 data) {
	// pixel data is in the two-most right bits
	data = data & 3;
	if (data == 1) {
		// the pixel is supposed to be red, we need to adjust
		data <<= 2;
	}
	return data;
}

LLDSPEC void gdisp_lld_flush(GDisplay *g) {
	// the display needs to awake from deep sleep, so we first check the current powerMode and if the display is not
	// sleeping, we need to put it to sleep and then awake it
	gdispGSetPowerMode(g, gPowerDeepSleep);
	gdispGSetPowerMode(g, gPowerOn);

	acquire_bus(g);
		
	for(int i=0; i<GDISP_SCREEN_HEIGHT; i++)
		for(int j=0; j<=((g->g.Width-1)/WS75bEPD_PPB); j++) {
			gU8 pixelValues = ((gU8 *)g->priv)[(GDISP_SCREEN_HEIGHT*j) + i];
			// as we are storing a pixel in 2 bits instead of 4 we need to extract that data now
			for (int k=0; k<2; ++k) {
				// put the pixels we are currently dealing with to the lower part
				pixelValues >>= k * 4;
				// first pixel is implemented in the third and fourth bit from the right (ENDIANESS!!)
				gU8 firstPixel = convertPixel(pixelValues >> 2);
				// the second pixel is in the most right bits
				gU8 secondPixel = convertPixel(pixelValues);
				gU8 data = (secondPixel << 4) | firstPixel;
				write_data(g, data);
			}
		}
		
	/* Update the screen. */
	write_cmd(g, DISPLAY_REFRESH);
	wait_until_idle(g);
	release_bus(g);

	// put display back to sleep
	gdispGSetPowerMode(g, gPowerDeepSleep);
}
#endif

#if GDISP_NEED_CONTROL && GDISP_HARDWARE_CONTROL
LLDSPEC void gdisp_lld_control(GDisplay *g) {
	switch(g->p.x) {
	case GDISP_CONTROL_POWER:
		if (g->g.Powermode == (gPowermode)g->p.ptr)
			return;
		switch((gPowermode)g->p.ptr) {
			case gPowerOff:
			case gPowerSleep:
			case gPowerDeepSleep:
				acquire_bus(g); // Put de display in deep sleep
				write_cmd(g, POWER_OFF);
				write_reg(g, DEEP_SLEEP_MODE, 0xA5);
				release_bus(g);
				break;
			case gPowerOn:
				resetDisplay(g);
				acquire_bus(g); // Awake the display again
				startUpDisplay(g);
				release_bus(g);
				break;
			default:
				return;
		}
		g->g.Powermode = (gPowermode)g->p.ptr;
		return;

  case GDISP_CONTROL_ORIENTATION:
		if (g->g.Orientation == (gOrientation)g->p.ptr)
			return;
		switch((gOrientation)g->p.ptr) {
		case gOrientation0:
			g->g.Height = GDISP_SCREEN_HEIGHT;
			g->g.Width = GDISP_SCREEN_WIDTH;
			break;
		case gOrientation90:
			g->g.Height = GDISP_SCREEN_WIDTH;
			g->g.Width = GDISP_SCREEN_HEIGHT;
			break;
		case gOrientation180:
			g->g.Height = GDISP_SCREEN_HEIGHT;
			g->g.Width = GDISP_SCREEN_WIDTH;
			break;
		case gOrientation270:
			g->g.Height = GDISP_SCREEN_WIDTH;
			g->g.Width = GDISP_SCREEN_HEIGHT;
			break;
		default:
			return;
		}
		g->g.Orientation = (gOrientation)g->p.ptr;
		return;
	default:
		return;
	}
}
#endif

#endif	// GFX_USE_GDISP
