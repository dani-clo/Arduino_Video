/*
    This file is part of the Arduino_Video library.

    Copyright (C) Arduino s.r.l. and/or its affiliated companies

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#if defined(__ZEPHYR__) && !defined(ARDUINO_GIGA)

#include <Arduino.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dsi.h>
#include "dsi.h"
#include "logging.h"

#define BYTES_PER_PIXEL 2

static uint32_t fb_size = 0;
static uint8_t *fb_base = NULL;

static const struct device *display_dev;
static const struct device *mipi_dsi_dev;

struct display_capabilities display_caps;

int dsi_init([[maybe_unused]]uint8_t bus, [[maybe_unused]]struct edid *edid, struct display_timing *dt) {
  int ret;

  /* Get LTDC device */
  display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
  if (!device_is_ready(display_dev)) {
    return -ENODEV;
  }

  /* Get framebuffer base from display driver */
  fb_base = (uint8_t *)display_get_framebuffer(display_dev);
  if (fb_base == NULL) {
    ANX_LOG_ERROR("dsi_init: framebuffer is NULL");
    return -ENOMEM;
  }
  ANX_LOG_INFO("dsi_init: framebuffer at %p", fb_base);

  /* Ensure resolution matches the overlay */
  display_get_capabilities(display_dev, &display_caps);
  if (display_caps.x_resolution != dt->hactive || display_caps.y_resolution != dt->vactive) {
    ANX_LOG_ERROR("dsi_init: Invalid display resolution");
    return -ENOTSUP;
  }
  ANX_LOG_INFO("dsi_init: display resolution %dx%d",
               display_caps.x_resolution, display_caps.y_resolution);

  fb_size = display_caps.x_resolution * display_caps.y_resolution * BYTES_PER_PIXEL;

  /* Get MIPI-DSI host device */
  mipi_dsi_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_mipi_dsi));
  if (!device_is_ready(mipi_dsi_dev)) {
    ANX_LOG_INFO("dsi_init: MIPI-DSI device not ready");
    return -ENODEV;
  }
  ANX_LOG_INFO("dsi_init: MIPI-DSI device ready");

  /* Configure MIPI-DSI device parameters */
  struct mipi_dsi_device mdev = {};
  mdev.data_lanes = 2;
  mdev.pixfmt = MIPI_DSI_PIXFMT_RGB565;
  mdev.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_LPM;

  /* Set timing from display_timing struct */
  mdev.timings.hactive = dt->hactive;
  mdev.timings.hfp = dt->hfront_porch;
  mdev.timings.hbp = dt->hback_porch;
  mdev.timings.hsync = dt->hsync_len;
  mdev.timings.vactive = dt->vactive;
  mdev.timings.vfp = dt->vfront_porch;
  mdev.timings.vbp = dt->vback_porch;
  mdev.timings.vsync = dt->vsync_len;

  ANX_LOG_INFO("dsi_init: timings ha=%d hfp=%d hbp=%d hsync=%d va=%d vfp=%d vbp=%d vsync=%d",
               mdev.timings.hactive, mdev.timings.hfp, mdev.timings.hbp, mdev.timings.hsync,
               mdev.timings.vactive, mdev.timings.vfp, mdev.timings.vbp, mdev.timings.vsync);

  /* Attach to DSI host on virtual channel 0 */
  ret = mipi_dsi_attach(mipi_dsi_dev, 0, &mdev);
  if (ret < 0) {
    ANX_LOG_INFO("dsi_init: mipi_dsi_attach failed ret=%d", ret);
    return ret;
  }
  ANX_LOG_INFO("dsi_init: mipi_dsi_attach success");

  /* Turn off blanking (enable display output) */
  [[maybe_unused]] int blank_ret = display_blanking_off(display_dev);
  ANX_LOG_INFO("dsi_init: display_blanking_off returned %d", blank_ret);

  dsi_lcdClear(0x00);
  dsi_drawCurrentFrameBuffer();

/* Dump registers for debugging */
#if defined(ANX_LOG_ENABLE)
  dsi_dump_registers();
#endif

  return 0;
}

uint32_t dsi_getDisplayXSize() {
  return display_caps.x_resolution;
}

uint32_t dsi_getDisplayYSize() {
  return display_caps.y_resolution;
}

/* Returns the back buffer for full-frame drawing + swap */
uint32_t dsi_getCurrentFrameBuffer() {
  uint8_t *front = (uint8_t *)display_get_framebuffer(display_dev);
  return (uint32_t)((front == fb_base) ? fb_base + fb_size : fb_base);
}

/* Returns the front buffer for partial/direct updates */
uint32_t dsi_getActiveFrameBuffer() {
  uint8_t *front = (uint8_t *)display_get_framebuffer(display_dev);
  return (uint32_t)front;
}

static inline uint16_t rgb888_to_rgb565(uint32_t rgb888) {
  uint8_t r = (rgb888 >> 16) & 0xFF;
  uint8_t g = (rgb888 >> 8) & 0xFF;
  uint8_t b = rgb888 & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void dsi_lcdClear(uint32_t Color) {
  uint32_t fb = dsi_getCurrentFrameBuffer();
  uint16_t *pixels = (uint16_t *)fb;
  uint16_t color16 = rgb888_to_rgb565(Color);

  for (uint32_t i = 0; i < display_caps.x_resolution * display_caps.y_resolution; i++) {
    pixels[i] = color16;
  }
}

void dsi_lcdFillArea(void *pDst, uint32_t xSize, uint32_t ySize, uint32_t ColorMode) {
  uint16_t *pixels = (uint16_t *)pDst;
  uint16_t color16 = rgb888_to_rgb565(ColorMode);

  for (uint32_t y = 0; y < ySize; y++) {
    for (uint32_t x = 0; x < xSize; x++) {
      pixels[y * display_caps.x_resolution + x] = color16;
    }
  }
}

void dsi_lcdDrawImage(void *pSrc, void *pDst, uint32_t xSize, uint32_t ySize, [[maybe_unused]]uint32_t ColorMode) {
  uint16_t *src = (uint16_t *)pSrc;
  uint16_t *dst = (uint16_t *)pDst;

  if (dst == NULL) {
    uint32_t fb = dsi_getCurrentFrameBuffer();
    dst = (uint16_t *)fb;
  }

  /* Copy line by line accounting for stride */
  for (uint32_t y = 0; y < ySize; y++) {
    memcpy(&dst[y * display_caps.x_resolution], &src[y * xSize], xSize * BYTES_PER_PIXEL);
  }
}

void dsi_configueCLUT([[maybe_unused]]uint32_t *colors) {
  /* CLUT not used in Zephyr display API with RGB565 */
}

uint32_t dsi_getFramebufferEnd(void) {
  if (fb_base == NULL) {
    return 0;
  }
  return (uint32_t)fb_base + CONFIG_STM32_LTDC_FB_NUM * fb_size;
}

void dsi_drawCurrentFrameBuffer(bool reload) {
  if (!reload) {
    return;
  }

  struct display_buffer_descriptor desc = {};
  desc.buf_size = fb_size;
  desc.width = display_caps.x_resolution;
  desc.height = display_caps.y_resolution;
  desc.pitch = display_caps.x_resolution;

  uint8_t *back = (uint8_t *)dsi_getCurrentFrameBuffer();

  // Zero-copy path: the LTDC driver uses this buffer directly as
  // pend_buf and waits for the vsync ISR to swap it with front_buf.
  display_write(display_dev, 0, 0, &desc, (void *)back);
}
#endif /* __ZEPHYR__ */
