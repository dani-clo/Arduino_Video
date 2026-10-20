/*
    This file is part of the Arduino_Video library.

    Copyright (C) Arduino s.r.l. and/or its affiliated companies

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Board capability macros */
#ifndef LED_BUILTIN
#define LED_BUILTIN LEDR
#endif

#if __has_include("HasIncludeArduinoGraphics.h")
#include "ArduinoGraphics.h"
#define ARDUINO_VIDEO_HAS_GRAPHICS
#endif

#if defined(CONFIG_INPUT_GT911)
#define ARDUINO_VIDEO_HAS_TOUCH
#endif

enum PlatformGpio {
  PLATFORM_GPIO_VIDEO_ON,
  PLATFORM_GPIO_VIDEO_RST,
  PLATFORM_GPIO_OTG_ON,
};

/**
 * @brief Initialize the platform peripherals (I2C and GPIOs).
 *
 * @return int 0 on success, otherwise an error code.
 */
int platformInit(void);

/**
 * @brief Set the state of a platform GPIO.
 *
 * @param gpio The platform GPIO to set.
 * @param state The state to apply (true for high, false for low).
 */
void platformGpioSet(PlatformGpio gpio, bool state);

/**
 * @brief Get the state of a platform GPIO.
 *
 * @param gpio The platform GPIO to read.
 * @return bool True if the GPIO is high, false otherwise.
 */
bool platformGpioGet(PlatformGpio gpio);

/**
 * @brief Set the direction of a platform GPIO.
 *
 * @param gpio The platform GPIO to configure.
 * @param output True to configure as output, false to configure as input.
 */
void platformGpioSetDirection(PlatformGpio gpio, bool output);

/**
 * @brief Write a value to a device register over I2C.
 *
 * @param saddr The 8-bit I2C slave address.
 * @param offset The register offset to write to.
 * @param val The value to write.
 * @return int 0 on success, otherwise an error code.
 */
int platformI2cWrite(uint8_t saddr, uint8_t offset, uint8_t val);

/**
 * @brief Read data from a device register over I2C.
 *
 * @param saddr The 8-bit I2C slave address.
 * @param offset The register offset to read from.
 * @param buf Pointer to the buffer that receives the data.
 * @param len The number of bytes to read.
 * @return int 0 on success, otherwise an error code.
 */
int platformI2cRead(uint8_t saddr, uint8_t offset, uint8_t *buf, size_t len);

/**
 * @brief Block execution for the given number of milliseconds.
 *
 * @param ms The number of milliseconds to wait.
 */
void platformDelayMs(uint32_t ms);

/**
 * @brief Start the periodic LVGL tick timer.
 */
void platformLvglStartTick(void);

#endif /* _PLATFORM_H */
