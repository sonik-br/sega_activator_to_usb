#pragma once

#include <Arduino.h>
#include "HID.h"

static const uint8_t _hidReportDescriptor[] PROGMEM = {
  0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
  0x09, 0x05,        // Usage (Game Pad)
  0xA1, 0x01,        // Collection (Application)
  0xA1, 0x00,        //   Collection (Physical)
  0x85, 0x01,        //     Report ID (1)
  0x05, 0x09,        //     Usage Page (Button)
  0x19, 0x01,        //     Usage Minimum (0x01)
  0x29, 0x10,        //     Usage Maximum (0x10)
  0x15, 0x00,        //     Logical Minimum (0)
  0x25, 0x01,        //     Logical Maximum (1)
  0x95, 0x10,        //     Report Count (16)
  0x75, 0x01,        //     Report Size (1)
  0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0xC0,              //   End Collection
  0xC0,              // End Collection
};

class GamePad_ {
  private:
    uint16_t _buttons;
    
  public:
    GamePad_(void) : _buttons(0) {
      static HIDSubDescriptor node(_hidReportDescriptor, sizeof(_hidReportDescriptor));
      HID().AppendDescriptor(&node);
    }

    void set_buttons(uint16_t b) {
      if (b != _buttons) {
        _buttons = b;
        HID().SendReport(1, &_buttons, sizeof(_buttons));
      }
    }

    void begin(void) {
      HID().SendReport(1, &_buttons, sizeof(_buttons));
    }
    void end(void) {}
};
