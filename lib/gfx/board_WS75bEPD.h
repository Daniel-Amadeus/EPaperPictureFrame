/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.io/license.html
 */


// definitions for Waveshare ESP32 Display Driver board

#ifndef GDISP_LLD_BOARD_H
#define GDISP_LLD_BOARD_H

#include <Arduino.h>
#include <gfx.h>
#include "WS75bEPD.h"

#define PIN_SPI_SCK  13
#define PIN_SPI_DIN  14
#define PIN_SPI_CS   15
#define PIN_SPI_BUSY 25
#define PIN_SPI_RST  26
#define PIN_SPI_DC   27

#define GPIO_PIN_SET   1
#define GPIO_PIN_RESET 0

static GFXINLINE void spi_transfer(gU8 data) {
    digitalWrite(PIN_SPI_CS, GPIO_PIN_RESET);

    for (int i=0; i<8; ++i) {
        if ((data & 0x80) == 0) digitalWrite(PIN_SPI_DIN, GPIO_PIN_RESET);
        else                    digitalWrite(PIN_SPI_DIN, GPIO_PIN_SET);
    
        data <<= 1;
        digitalWrite(PIN_SPI_SCK, GPIO_PIN_SET);
        digitalWrite(PIN_SPI_SCK, GPIO_PIN_RESET);
    }

    digitalWrite(PIN_SPI_CS, GPIO_PIN_SET);
}

static GFXINLINE void init_board(GDisplay *g) {
    pinMode(PIN_SPI_BUSY, INPUT);
    pinMode(PIN_SPI_RST, OUTPUT);
    pinMode(PIN_SPI_DC, OUTPUT);

    pinMode(PIN_SPI_SCK, OUTPUT);
    pinMode(PIN_SPI_DIN, OUTPUT);
    pinMode(PIN_SPI_CS, OUTPUT);

    digitalWrite(PIN_SPI_CS, HIGH);
    digitalWrite(PIN_SPI_SCK, LOW);
}

static GFXINLINE void post_init_board(GDisplay *g) {
	(void) g;
}

static GFXINLINE void setpin_reset(GDisplay *g, gBool state) {
	if (state) {
        digitalWrite(PIN_SPI_RST, HIGH);
    } else {
        digitalWrite(PIN_SPI_RST, LOW);
    }
}

static GFXINLINE void acquire_bus(GDisplay *g) {
	(void) g;
}

static GFXINLINE void release_bus(GDisplay *g) {
	(void) g;
}

static GFXINLINE void write_data(GDisplay *g, gU8 data) {
	digitalWrite(PIN_SPI_DC, HIGH);
    spi_transfer(data);
}

static GFXINLINE void wait_until_idle(GDisplay *g) {
    while(digitalRead(PIN_SPI_BUSY) == 0) gfxSleepMilliseconds(100);  
}

static GFXINLINE void write_cmd(GDisplay *g, gU8 reg){
    digitalWrite(PIN_SPI_DC, LOW);
    spi_transfer(reg);
}

static GFXINLINE void write_reg(GDisplay *g, gU8 reg, gU8 data){
    write_cmd(g, reg);
    write_data(g, data);
}

static GFXINLINE void write_reg_data(GDisplay *g, gU8 reg, gU8 *data, gU8 len) {
    write_cmd(g, reg);
    for (int i=0; i<len; ++i) {
        write_data(g, *data);
        data++;
    }
}

#endif /* GDISP_LLD_BOARD_H */
