/*
  Camera to Display

  Requirements:
  - An Arduino board with both a supported camera and a display, for example a
    GIGA R1 with the GIGA Display Shield, or Portenta H7 with a GC2145 camera
    and a monitor connected via USB-C.

  Shows the camera output on the display, scaled up by whatever whole number
  fits and centred in the display framebuffer. On GIGA, each frame is also
  rotated 90 degrees clockwise to match the portrait panel.

  How to use this example:

  1. Change CAMERA_WIDTH / CAMERA_HEIGHT below if you want a different mode.
     The rotation, scale factor and centring all follow from it.

  2. Upload this sketch to the board.

  The frame rate is printed to Serial once a second. On GIGA, most of the frame
  time goes on the rotation, since neither the LTDC nor DMA2D can transpose.
*/

#include "camera.h"
#include "Arduino_Video.h"

// Capture settings.
constexpr int CAMERA_WIDTH  = 320;
constexpr int CAMERA_HEIGHT = 240;

#if CONFIG_STM32_LTDC_FB_NUM < 1
#error "Set CONFIG_STM32_LTDC_FB_NUM to 1 or more."
#endif

#if defined(ARDUINO_GIGA)
// Use native portrait coordinates; the camera frame is rotated below.
Arduino_Video Display(480, 800, GigaDisplayShield);
#elif defined(ARDUINO_PORTENTA_H7_M7)
// Match the LTDC resolution configured in the Portenta H7 overlay.
Arduino_Video Display(1024, 768, USBCVideo);
#else
#error "CameraDisplay supports GIGA R1 and Portenta H7."
#endif

static int display_width;
static int display_height;
static int scale;
static int out_x;
static int out_y;

#ifdef ARDUINO_GIGA
// Rotating 90 degrees swaps the axes.
constexpr int FRAME_WIDTH = CAMERA_HEIGHT;
constexpr int FRAME_HEIGHT = CAMERA_WIDTH;
#else
constexpr int FRAME_WIDTH = CAMERA_WIDTH;
constexpr int FRAME_HEIGHT = CAMERA_HEIGHT;
#endif

// The camera and display buffers both live in external SDRAM, where a strided
// write costs a row activation. Walking the frame in tiles keeps each tile's
// reads and writes inside a few open rows.
constexpr int TILE = 16;

Camera cam;

void fatal_error(const char *msg) {
  pinMode(LED_BUILTIN, OUTPUT);
  while (1) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    Serial.println(msg);
  }
}

/*
 * Scale up by the largest whole number that fits the display, optionally
 * rotating 90 degrees clockwise. The rotated path uses tiles to keep strided
 * SDRAM reads and writes close together; the landscape path writes rows.
 */
static void rotate_and_scale(const uint16_t *src, uint16_t *dst, bool rotate = false) {
  if (!rotate) {
    for (int y = 0; y < CAMERA_HEIGHT; y++) {
      const uint16_t *s = src + y * CAMERA_WIDTH;
      for (int sy = 0; sy < scale; sy++) {
        uint16_t *d = dst + (out_y + y * scale + sy) * display_width + out_x;
        for (int x = 0; x < CAMERA_WIDTH; x++) {
          uint16_t pixel = s[x];
          for (int sx = 0; sx < scale; sx++) {
            *d++ = pixel;
          }
        }
      }
    }
    return;
  }

  for (int tx = 0; tx < CAMERA_HEIGHT; tx += TILE) {
    int x_end = MIN(tx + TILE, CAMERA_HEIGHT);

    for (int ty = 0; ty < CAMERA_WIDTH; ty += TILE) {
      int y_end = MIN(ty + TILE, CAMERA_WIDTH);

      for (int x = tx; x < x_end; x++) {
        const uint16_t *s = src + (CAMERA_HEIGHT - 1 - x) * CAMERA_WIDTH + ty;
        uint16_t *d = dst + (out_y + ty * scale) * display_width + out_x + x * scale;

        for (int y = ty; y < y_end; y++) {
          uint16_t pixel = *s++;

          for (int sy = 0; sy < scale; sy++) {
            for (int sx = 0; sx < scale; sx++) {
              d[sy * display_width + sx] = pixel;
            }
          }

          d += display_width * scale;
        }
      }
    }
  }
}

void setup(void) {
  Serial.begin(115200);

  if (!cam.begin(CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_RGB565, true)) {
    fatal_error("Camera begin failed");
  }

  cam.setVerticalFlip(false);
  cam.setHorizontalMirror(false);

  if (Display.begin() != 0) {
    fatal_error("Display initialization failed");
  }

  display_width = Display.width();
  display_height = Display.height();
  scale = MIN(display_width / FRAME_WIDTH, display_height / FRAME_HEIGHT);
  if (scale < 1) {
    fatal_error("Camera frame is larger than the display");
  }
  out_x = (display_width - FRAME_WIDTH * scale) / 2;
  out_y = (display_height - FRAME_HEIGHT * scale) / 2;

  Serial.print("camera ");
  Serial.print(CAMERA_WIDTH);
  Serial.print("x");
  Serial.print(CAMERA_HEIGHT);
  Serial.print(" -> display ");
  Serial.print(display_width);
  Serial.print("x");
  Serial.print(display_height);
#ifdef ARDUINO_GIGA
  Serial.print(" rotated,");
#endif
  Serial.print(" scale ");
  Serial.println(scale);
}

void loop() {
  static unsigned long last_report = 0;
  static unsigned int frames = 0;
  FrameBuffer fb;

  if (!cam.grabFrame(fb, 100)) {
    return;
  }

  uint16_t *dst = (uint16_t *)Display.getDrawBuffer();
  if (dst == nullptr) {
    cam.releaseFrame(fb);
    fatal_error("Display RGB565 buffer unavailable");
  }
#ifdef ARDUINO_GIGA
  rotate_and_scale((const uint16_t *)fb.getBuffer(), dst, true);
#else
  rotate_and_scale((const uint16_t *)fb.getBuffer(), dst);
#endif
  cam.releaseFrame(fb);

  if (Display.drawBuffer(0, 0, dst) != 0) {
    fatal_error("Display update failed");
  }

  frames++;
  if (millis() - last_report >= 1000) {
    Serial.print(frames);
    Serial.println(" fps");
    frames = 0;
    last_report = millis();
  }
}

