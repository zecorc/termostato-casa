#pragma once

// TFT_eSPI setup — HW-394 (ESP32-WROOM) + GMT028-05 2.8" SPI 240x320 (ILI9341, no touch)
// Wired: MOSI=D23, SCK=D18, CS=D15, DC=D21, RST=D4, VCC=3V3, GND=GND

#define USER_SETUP_INFO "termostato HW-394 GMT028-05"

#define ILI9341_DRIVER

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 15
#define TFT_DC 21
#define TFT_RST 4

#define TOUCH_CS -1  // no touch panel on GMT028-05 7-pin module

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000
