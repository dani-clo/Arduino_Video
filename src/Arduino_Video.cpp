/*
    This file is part of the Arduino_Video library.

    Copyright (C) Arduino s.r.l. and/or its affiliated companies

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#include "Arduino_Video.h"

#include "dsi.h"
#include "logging.h"
extern "C" {
#include "video_modes.h"
}

#if defined(__ZEPHYR__) && defined(ARDUINO_GIGA)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#endif

#if __has_include("lvgl.h")
#include "lvgl.h"

#ifndef LVGL_VERSION_MAJOR
#error "LVGL_VERSION_MAJOR is not defined. Please include a valid LVGL v9.x.x installation."
#endif

#if LVGL_VERSION_MAJOR < 9
#error "Arduino_Video library supports only LVGL version 9.x.x or higher."
#endif
#endif

#if __has_include("lvgl.h")
#if defined(__ZEPHYR__)
#include "platform.h"
void lvgl_displayFlushing(lv_display_t *display, const lv_area_t *area, unsigned char *px_map);
#endif
#endif /* __has_include ("lvgl.h") */

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
/* Note: These variables are defined in the global scope because the LVGL
 *       'lvgl_displayFlushing' callback is a static function */
const struct device *display_dev;
struct display_capabilities display_caps = {};
static struct display_buffer_descriptor desc = {
  .buf_size = 0,
  .width = 0,
  .height = 0,
  .pitch = 0,
  .frame_incomplete = false
};
uint16_t *buffer = nullptr;
#endif

Arduino_Video::Arduino_Video(int width, int height, DisplayShield &shield)
#ifdef ARDUINO_VIDEO_HAS_GRAPHICS
  : ArduinoGraphics(width, height)
#endif
{
  _height = height;
  _width = width;
  _shield = &shield;
  _edidMode = _shield->getEdidMode(width, height);

  switch (_edidMode) {
    case EDID_MODE_640x480_60Hz ... EDID_MODE_800x600_59Hz:
    case EDID_MODE_1024x768_60Hz ... EDID_MODE_1920x1080_60Hz:
      _rotated = (width < height) ? true : false;
      break;
    case EDID_MODE_480x800_60Hz:
      _rotated = (width >= height) ? true : false;
      break;
    default:
      _rotated = false;
      break;
  }
}

Arduino_Video::~Arduino_Video() {
}

int Arduino_Video::begin() {
  ANX_LOG_INFO("Arduino_Video::begin() called");

#ifdef ARDUINO_VIDEO_HAS_GRAPHICS
  if (!ArduinoGraphics::begin()) {
    return 1; /* Unknown err */
  }

  textFont(Font_5x7);
#endif

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)  // Backend for Arduino Giga R1
  display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

  if (!device_is_ready(display_dev)) {
    printk("\t<err> Zephyr Display Not Ready!...");
    return 3;
  }

  display_get_capabilities(display_dev, &display_caps);
  _width = display_caps.y_resolution;
  _height = display_caps.x_resolution;

  printk("- Capabilities:\n");
  printk("  x_resolution = %u, y_resolution = %u\n, supported_pixel_formats = %u\n"
         "  current_pixel_format = %u, current_orientation = %u\n",
         display_caps.x_resolution, display_caps.y_resolution,
         display_caps.supported_pixel_formats, display_caps.current_pixel_format,
         display_caps.current_orientation);

  display_blanking_off(display_dev);

  void *ptrFB = display_get_framebuffer(display_dev);
  if (ptrFB == nullptr) {
    printk("Memory display allocation failed!");
    while (1) {}
  }
  // Cast the void pointer to an int pointer to use it
  buffer = static_cast<uint16_t *>(ptrFB);

  display_blanking_on(display_dev);

  setFrameDesc(width(), height(), width(), (width() * height() * sizeof(uint16_t)));
#else
  /* Video controller/bridge init */
  int err_code = _shield->init(_edidMode);
  if (err_code < 0) {
    return 3; /* Video controller fail init */
  }
#endif

#if __has_include("lvgl.h")
  /* Initialize LVGL library */
  lv_init();

  /* Create a draw buffer */
  static lv_color_t *buf1 = (lv_color_t *)malloc((width() * height() / 10)); /* Declare a buffer for 1/10 screen size */
  if (buf1 == NULL) {
    return 2; /* Insuff memory err */
  }

  lv_display_t *display;
  if (_rotated) {
    display = lv_display_create(height(), width());
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);
    //display->sw_rotate = 1;
  } else {
    display = lv_display_create(width(), height());
  }
  lv_display_set_buffers(display, buf1, NULL, width() * height() / 10, LV_DISPLAY_RENDER_MODE_PARTIAL); /*Initialize the display buffer.*/
  lv_display_set_flush_cb(display, lvgl_displayFlushing);
#if __ZEPHYR__
  platformLvglStartTick();
#endif
  printk("LVGL Correctly Initialized\n");
#endif

  // turn on the display backlight
  pinMode(74, OUTPUT);  // 74 = PB_12
  digitalWrite(74, HIGH);

  return 0;
}

int Arduino_Video::width() {
  return _width;
}

int Arduino_Video::height() {
  return _height;
}

bool Arduino_Video::isRotated() {
  return _rotated;
}

bool Arduino_Video::detect() {
  return (_shield->getStatus() > 0);
}

void Arduino_Video::end() {
#ifdef ARDUINO_VIDEO_HAS_GRAPHICS
  ArduinoGraphics::end();
#endif
}

#ifdef ARDUINO_VIDEO_HAS_GRAPHICS
void Arduino_Video::beginDraw() {
  ArduinoGraphics::beginDraw();

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  uint16_t *fb = (uint16_t *)display_get_framebuffer(display_dev);
  memset(fb, 0, display_caps.x_resolution * display_caps.y_resolution * sizeof(uint16_t));
#else
  dsi_lcdClear(0);
#endif
}

void Arduino_Video::endDraw() {
  ArduinoGraphics::endDraw();

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  // no need to explicitly flush, as we are writing directly to the framebuffer
#else
  dsi_drawCurrentFrameBuffer();
#endif
}

void Arduino_Video::clear() {
  [[maybe_unused]] uint32_t bg = ArduinoGraphics::background();
  [[maybe_unused]] uint32_t x_size, y_size;

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  uint16_t *fb = (uint16_t *)display_get_framebuffer(display_dev);
  memset(fb, 0, display_caps.x_resolution * display_caps.y_resolution * 2);
#else
  if (_rotated) {
    x_size = ((uint32_t)height() <= dsi_getDisplayXSize()) ? height() : dsi_getDisplayXSize();
    y_size = ((uint32_t)width() <= dsi_getDisplayYSize()) ? width() : dsi_getDisplayYSize();
  } else {
    x_size = ((uint32_t)width() <= dsi_getDisplayXSize()) ? width() : dsi_getDisplayXSize();
    y_size = ((uint32_t)height() <= dsi_getDisplayYSize()) ? height() : dsi_getDisplayYSize();
  }

  dsi_lcdFillArea((void *)(dsi_getCurrentFrameBuffer()), x_size, y_size, bg);
#endif
}

void Arduino_Video::set(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t x_rot, y_rot;

  if (_rotated) {
    x_rot = ((height() - 1) - y);
    y_rot = x;

    if (x_rot >= (uint32_t)height() || y_rot >= (uint32_t)width())
      return;
  } else {
    x_rot = x;
    y_rot = y;

    if (x_rot >= (uint32_t)width() || y_rot >= (uint32_t)height())
      return;
  }

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  if (x_rot >= display_caps.x_resolution || y_rot >= display_caps.y_resolution)
    return;
#else
  if (x_rot >= dsi_getDisplayXSize() || y_rot >= dsi_getDisplayYSize())
    return;
#endif

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  // The pixel is written directly into the framebuffer in RGB565 format.
  // Rotation is automatically handled if the display is rotated.
  uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  uint16_t *fb = (uint16_t *)display_get_framebuffer(display_dev);
  fb[x_rot + (display_caps.x_resolution * y_rot)] = color;
#else
  // The pixel is written via the DSI controller to the active frame buffer.
  uint32_t color = (r << 16) | (g << 8) | b;
  dsi_lcdFillArea((void *)(dsi_getCurrentFrameBuffer() + ((x_rot + (dsi_getDisplayXSize() * y_rot)) * sizeof(uint16_t))), 1, 1, color);
#endif
}
#endif

#if __has_include("lvgl.h")
static uint8_t *rotated_buf = nullptr;
void lvgl_displayFlushing(lv_display_t *disp, const lv_area_t *area, unsigned char *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  lv_area_t *area_in_use = (lv_area_t *)area;

  // TODO: find a smart way to tackle sw rotation
  lv_display_rotation_t rotation = lv_display_get_rotation(disp);
  lv_area_t rotated_area;
  if (rotation != LV_DISPLAY_ROTATION_0) {
    rotated_buf = (uint8_t *)realloc(rotated_buf, w * h * 4);
    lv_color_format_t cf = lv_display_get_color_format(disp);
#if (LVGL_VERSION_MINOR < 2)
    rotation = LV_DISPLAY_ROTATION_90;  // bugfix: force 90 degree rotation for lvgl 9.1 end earlier
#endif
    lv_draw_sw_rotate(px_map, rotated_buf,
                      w, h, lv_draw_buf_width_to_stride(w, cf),
                      lv_draw_buf_width_to_stride(h, cf),
                      rotation, cf);

    rotated_area.x1 = lv_display_get_vertical_resolution(disp) - area->y2 - 1;
    rotated_area.y1 = area->x1;
    //rotated_area.y2 = dsi_getDisplayYSize() - area->x1 - 1;
    rotated_area.x2 = rotated_area.x1 + h - 1;
    rotated_area.y2 = rotated_area.y1 + w + 1;

    area_in_use = &rotated_area;
    px_map = rotated_buf;
    auto temp = w;
    w = h;
    h = temp;
  }

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  uint16_t *dst = buffer;
  uint16_t *src = (uint16_t *)px_map;

  for (uint32_t y = 0; y < h; y++) {
    uint32_t dst_idx = (area_in_use->y1 + y) * display_caps.x_resolution + area_in_use->x1;
    memcpy(&dst[dst_idx], &src[y * w], w * sizeof(uint16_t));
  }
#else
  uint32_t offsetPos = (area_in_use->x1 + (dsi_getDisplayXSize() * area_in_use->y1)) * sizeof(uint16_t);
  dsi_lcdDrawImage((void *)px_map, (void *)(dsi_getActiveFrameBuffer() + offsetPos), w, h, DMA2D_INPUT_RGB565);
#endif

  lv_display_flush_ready(disp); /* Indicate you are ready with the flushing*/
}
#endif  //end lvgl

int Arduino_Video::drawBuffer([[maybe_unused]]uint16_t x, [[maybe_unused]]uint16_t y, [[maybe_unused]]const void *buf) {
#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  if (!device_is_ready(display_dev)) {
    return -ENODEV;
  }
  return display_write(display_dev, x, y, &desc, buf);
#else
  return -1; /* Fallback for other platforms if not yet implemented */
#endif
}

void *Arduino_Video::getFramebuffer() {
#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
  /* * In Zephyr, 'buffer' is the pointer allocated or retrieved from the display device during begin() */
  void *fb = display_get_framebuffer(display_dev);
  return fb;
#else
  return nullptr;
#endif
}

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
void Arduino_Video::setFrameDesc(uint16_t w, uint16_t h, uint16_t pitch, uint32_t buf_size) {
  desc.buf_size = buf_size;
  desc.width = w;     /** Number of pixels between consecutive rows in the data buffer */
  desc.height = h;    /** Data buffer row width in pixels */
  desc.pitch = pitch; /** Data buffer row height in pixels */
}
#endif
