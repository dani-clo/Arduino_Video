/*
    This file is part of the Arduino_Video library.

    Copyright (C) Arduino s.r.l. and/or its affiliated companies

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#include "DisplayShield.h"

#include "Arduino.h"
#include "anx7625.h"
extern "C" {
#include "video_modes.h"
}

#if defined(ARDUINO_GIGA) && defined(__ZEPHYR__)
int GigaDisplayShieldClass::init([[maybe_unused]]int edidmode) {
  return 0;
}

int GigaDisplayShieldClass::getEdidMode([[maybe_unused]]int h, [[maybe_unused]] int v) {
  return EDID_MODE_480x800_60Hz;
}

int GigaDisplayShieldClass::getStatus() {
  return 1;  // TODO: Not implemented;
}

GigaDisplayShieldClass GigaDisplayShield;
#endif /* ARDUINO_GIGA && __ZEPHYR__ */

int USBCVideoClass::init(int edidmode) {
  struct edid recognized_edid;
  int err_code = 0;

  memset(&recognized_edid, 0, sizeof(recognized_edid));

  //Initialization of ANX7625
  err_code = anx7625_init(0);
  if (err_code < 0) {
    return err_code;
  }

  //Checking HDMI plug event
  err_code = anx7625_wait_hpd_event(0);
  if (err_code < 0) {
    return err_code;
  }

  //Read EDID
  anx7625_dp_get_edid(0, &recognized_edid);

  //DSI Configuration
  err_code = anx7625_dp_start(0, &recognized_edid, (enum edid_modes)edidmode);
  if (err_code < 0) {
    return err_code;
  }

  return 0;
}

int USBCVideoClass::getEdidMode(int h, int v) {
  int edidmode = video_modes_get_edid(h, v);

  return edidmode;
}

int USBCVideoClass::getStatus() {
  int detected = anx7625_get_hpd_event(0);

  return detected;
}

USBCVideoClass USBCVideo;
