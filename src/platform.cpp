/*
    This file is part of the Arduino_Video library.

    Copyright (C) Arduino s.r.l. and/or its affiliated companies

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyrPinctrl.h>

#include "platform.h"
#include "logging.h"

#if __has_include("lvgl.h")
#include "lvgl.h"
#endif

#if defined(ARDUINO_PORTENTA_H7_M7)

/* GPIO specifications from devicetree */
#define ANX7625_NODE DT_NODELABEL(anx7625)

static const struct device *i2c_dev = DEVICE_DT_GET(DT_PHANDLE(ANX7625_NODE, i2c_bus));
static const struct gpio_dt_spec video_on_gpio = GPIO_DT_SPEC_GET(ANX7625_NODE, video_on_gpios);
static const struct gpio_dt_spec video_rst_gpio = GPIO_DT_SPEC_GET(ANX7625_NODE, video_rst_gpios);
static const struct gpio_dt_spec otg_on_gpio = GPIO_DT_SPEC_GET(ANX7625_NODE, otg_on_gpios);

int platformInit(void) {
  int ret;

  /* Init the I2C device and re-apply DEFAULT pinctrl state so shared
     * pins are remuxed back to I2C after other peripherals have used them.
     */
  (void)zephyr::arduino::init_dev_apply_pinctrl(i2c_dev);

  /* Check I2C device ready */
  if (!device_is_ready(i2c_dev)) {
    ANX_LOG_ERROR("I2C device not ready");
    return -ENODEV;
  }

  /* Configure video_on as output (initially low) */
  if (!gpio_is_ready_dt(&video_on_gpio)) {
    ANX_LOG_ERROR("video_on GPIO not ready");
    return -ENODEV;
  }
  ret = gpio_pin_configure_dt(&video_on_gpio, GPIO_OUTPUT_LOW);
  if (ret < 0) {
    ANX_LOG_ERROR("Failed to configure video_on GPIO: %d", ret);
    return ret;
  }

  /* Configure video_rst as output (initially low) */
  if (!gpio_is_ready_dt(&video_rst_gpio)) {
    ANX_LOG_ERROR("video_rst GPIO not ready");
    return -ENODEV;
  }
  ret = gpio_pin_configure_dt(&video_rst_gpio, GPIO_OUTPUT_LOW);
  if (ret < 0) {
    ANX_LOG_ERROR("Failed to configure video_rst GPIO: %d", ret);
    return ret;
  }

  /* Configure otg_on as input with pull-up (initially) */
  if (!gpio_is_ready_dt(&otg_on_gpio)) {
    ANX_LOG_ERROR("otg_on GPIO not ready");
    return -ENODEV;
  }
  ret = gpio_pin_configure_dt(&otg_on_gpio, GPIO_INPUT | GPIO_PULL_UP);
  if (ret < 0) {
    ANX_LOG_ERROR("Failed to configure otg_on GPIO: %d", ret);
    return ret;
  }

  return 0;
}

void platformGpioSet(PlatformGpio gpio, bool state) {
  switch (gpio) {
    case PLATFORM_GPIO_VIDEO_ON:
      gpio_pin_set_dt(&video_on_gpio, state ? 1 : 0);
      break;
    case PLATFORM_GPIO_VIDEO_RST:
      gpio_pin_set_dt(&video_rst_gpio, state ? 1 : 0);
      break;
    case PLATFORM_GPIO_OTG_ON:
      if (gpio_pin_is_output_dt(&otg_on_gpio)) {
        gpio_pin_set_dt(&otg_on_gpio, state ? 1 : 0);
      }
      break;
  }
}

bool platformGpioGet(PlatformGpio gpio) {
  switch (gpio) {
    case PLATFORM_GPIO_VIDEO_ON:
      return gpio_pin_get_dt(&video_on_gpio) != 0;
    case PLATFORM_GPIO_VIDEO_RST:
      return gpio_pin_get_dt(&video_rst_gpio) != 0;
    case PLATFORM_GPIO_OTG_ON:
      return gpio_pin_get_dt(&otg_on_gpio) != 0;
    default:
      return false;
  }
}

void platformGpioSetDirection(PlatformGpio gpio, bool output) {
  switch (gpio) {
    case PLATFORM_GPIO_OTG_ON:
      if (output) {
        gpio_pin_configure_dt(&otg_on_gpio, GPIO_OUTPUT);
      } else {
        gpio_pin_configure_dt(&otg_on_gpio, GPIO_INPUT | GPIO_PULL_UP);
      }
      break;
    default:
      break;
  }
}

int platformI2cWrite(uint8_t saddr, uint8_t offset, uint8_t val) {
  uint8_t buf[2] = { offset, val };
  /* Zephyr uses 7-bit address, mbed uses 8-bit */
  return i2c_write(i2c_dev, buf, 2, saddr >> 1);
}

int platformI2cRead(uint8_t saddr, uint8_t offset, uint8_t *buf, size_t len) {
  /* Zephyr uses 7-bit address, mbed uses 8-bit */
  return i2c_write_read(i2c_dev, saddr >> 1, &offset, 1, buf, len);
}

void platformDelayMs(uint32_t ms) {
  k_msleep(ms);
}

#endif /* ARDUINO_PORTENTA_H7_M7 */

void platformLvglStartTick(void) {
#if __has_include("lvgl.h")
#if (LVGL_VERSION_MAJOR == 9)
  static struct k_timer lvgl_tick_timer;
  static bool started = false;
  if (!started) {
    k_timer_init(&lvgl_tick_timer, [](struct k_timer *timer) {
      ARG_UNUSED(timer);
      lv_tick_inc(16);
    },
                 NULL);
    k_timer_start(&lvgl_tick_timer, K_MSEC(16), K_MSEC(16));
    started = true;
  }
#endif
#endif
}
