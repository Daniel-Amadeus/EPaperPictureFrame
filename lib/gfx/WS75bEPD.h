/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */

#ifndef _WS75bEPD_H_
#define _WS75bEPD_H_

/* Register definitions. */
#define POWER_SETTING                   0x01
#define PANEL_SETTING                   0x00
#define POWER_ON                        0x04
#define BOOSTER_SOFT_START              0x06

#define PLL_CONTROL                     0x30
#define TEMP_SENSOR_CTRL                0x41
#define VCOM_AND_DATA_INTERVAL_SETTING  0x50
#define TCON_SETTING                    0x60
#define TCON_RESOLUTION                 0x61
#define VCM_DC_SETTING                  0x82
#define FLASH_MODE                      0xE5

#define DATA_START_TRANSMISSION_1       0x10

#define POWER_OFF                       0x02
#define DEEP_SLEEP_MODE                 0x07

/* Command definitions */
#define DISPLAY_REFRESH                 0x12

#endif
