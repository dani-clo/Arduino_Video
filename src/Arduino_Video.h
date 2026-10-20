/*
    This file is part of the Arduino_Video library.

    Copyright (C) Arduino s.r.l. and/or its affiliated companies

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#ifndef _ARDUINO_VIDEO_H
#define _ARDUINO_VIDEO_H

#include "platform.h"
#include "DisplayShield.h"

/**
 * @class Arduino_Video
 * @brief The main class for managing the video controller and the display.
 */
class Arduino_Video
#ifdef ARDUINO_VIDEO_HAS_GRAPHICS
  : public ArduinoGraphics
#endif
{
public:
  /**
   * @brief Constructs a new Arduino_Video object with the specified width, height, and display shield.
   *
   * @param width The width of the display.
   * @param height The height of the display.
   * @param shield The display shield used.
   *               - GigaDisplayShield: Giga Display Shield
   *               - USBCVideo: Display attach to the USB-C port
   */
#if defined(ARDUINO_PORTENTA_H7_M7)
  Arduino_Video(int width = 1024, int height = 768, DisplayShield &shield = USBCVideo);
#elif defined(ARDUINO_GIGA)
  Arduino_Video(int width = 800, int height = 480, DisplayShield &shield = GigaDisplayShield);
#else
#error "Arduino_Video: unsupported board or core configuration"
#endif

  /**
   * @brief Destructor for the Arduino_Video object.
   */
  ~Arduino_Video();

  /**
   * @brief Initialize the video controller and display.
   *
   * @return int 0 if initialization is successful, otherwise an error code.
   */
  int begin();

  /**
   * @brief De-initialize the video controller and display.
   */
  void end();

  /**
   * @brief Get the width of the display.
   *
   * @return int The width of the display.
   */
  int width();

  /**
   * @brief Get the height of the display.
   *
   * @return int The height of the display.
   */
  int height();

  /**
   * @brief Check if the display is rotated.
   *
   * @return bool True if the display is rotated, false otherwise.
   */
  bool isRotated();

  /**
   * @brief Checks if the display is connected.
   *
   * @return true if the display is connected, false otherwise.
   */
  bool detect();

  /**
   * @brief Draw a buffer to the display at the specified coordinates.
   *
   * @param x The x-coordinate of the top-left corner where the buffer will be drawn.
   * @param y The y-coordinate of the top-left corner where the buffer will be drawn.
   * @param buf A pointer to the buffer containing the pixel data.
   * @return int 0 if the buffer is successfully drawn, otherwise an error code.
   */
  int drawBuffer(uint16_t x, uint16_t y, const void *buf);

  /**
   * @brief Get a pointer to the framebuffer.
   *
   * @return void* A pointer to the framebuffer, or nullptr if the framebuffer is not available.
   */
  void *getFramebuffer();

  /**
   * @brief Set the frame descriptor for the display.
   *
   * @param w The width of the frame in pixels.
   * @param h The height of the frame in pixels.
   * @param pitch The number of pixels between consecutive rows in the data buffer.
   * @param buf_size The size of the data buffer in bytes.
   */
  void setFrameDesc(uint16_t w, uint16_t h, uint16_t pitch, uint32_t buf_size);
#ifdef ARDUINO_VIDEO_HAS_GRAPHICS
  /**
   * @brief Clear the display.
   */
  void clear();

  /**
   * @brief Begin drawing operations on the display.
   */
  virtual void beginDraw();

  /**
   * @brief End drawing operations on the display.
   */
  virtual void endDraw();

  /**
   * @brief Set the color of the pixel at the specified coordinates.
   *
   * @param x The x-coordinate of the pixel.
   * @param y The y-coordinate of the pixel.
   * @param r The red component of the color.
   * @param g The green component of the color.
   * @param b The blue component of the color.
   */
  virtual void set(int x, int y, uint8_t r, uint8_t g, uint8_t b);
#endif
private:
  DisplayShield *_shield;
  bool _rotated;
  int _edidMode;
  uint32_t _width;
  uint32_t _height;
};

#endif /* _ARDUINO_VIDEO_H */
