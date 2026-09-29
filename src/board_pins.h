#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

// Pin map for the Waveshare ESP32-C6 1.54" e-Paper board, taken from
// Waveshare's ESP-IDF examples (epaper_config.h). These are fixed by the board.

// e-Paper panel (SPI)
#define EPD_SPI_HOST SPI2_HOST
#define EPD_SCK_PIN GPIO_NUM_6
#define EPD_MOSI_PIN GPIO_NUM_5
#define EPD_CS_PIN GPIO_NUM_7
#define EPD_DC_PIN GPIO_NUM_15
#define EPD_RST_PIN GPIO_NUM_11
#define EPD_BUSY_PIN GPIO_NUM_10

// I2C bus shared by the TCA9554 expander, PCF85063 RTC and SHTC3 sensor
#define BOARD_I2C_PORT I2C_NUM_0
#define BOARD_I2C_SDA_PIN GPIO_NUM_18
#define BOARD_I2C_SCL_PIN GPIO_NUM_8

// TCA9554 I/O expander and the pins it drives
#define TCA9554_I2C_ADDRESS 0x20
#define EXIO_EPD_POWER 0
#define EXIO_BATTERY_HOLD 5
