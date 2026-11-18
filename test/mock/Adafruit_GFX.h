#pragma once
// Host-side Adafruit_GFX shim for native UI tests: just the drawing surface
// infra/display/renderer.cpp uses. Pixels are discarded; text is recorded so
// tests can compare whole frames. `print_hook` (if set) runs once after the
// next text call, letting a test change state mid-draw the way the 1 kHz ISR
// can on hardware.
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

class Adafruit_GFX {
public:
  Adafruit_GFX(int16_t, int16_t) {}
  virtual ~Adafruit_GFX() {}
  virtual void drawPixel(int16_t, int16_t, uint16_t) {}
  void fillScreen(uint16_t) {}
  void setTextColor(uint16_t) {}
  void setCursor(int16_t, int16_t) { text += '|'; }
  void fillRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void drawRect(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  void drawLine(int16_t, int16_t, int16_t, int16_t, uint16_t) {}
  size_t write(uint8_t c) { text += (char)c; fire_hook(); return 1; }
  size_t printf(const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    text += buf;
    fire_hook();
    return n < 0 ? 0 : (size_t)n;
  }

  std::string text;                 // everything printed since the last clear
  void (*print_hook)() = nullptr;   // one-shot, cleared before it runs

private:
  void fire_hook() {
    if (!print_hook) return;
    void (*h)() = print_hook;
    print_hook = nullptr;
    h();
  }
};
