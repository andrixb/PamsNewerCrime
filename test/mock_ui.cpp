// UI port fakes for the native test build: lets the REAL controller
// (infra/display/ui_controller.cpp) and renderer (infra/display/renderer.cpp)
// run on the host. The OLED is a text-recording Adafruit_GFX mock; the saver
// and splash are inert so they never swallow a test's input.
#include "infra/display/oled.h"
#include "infra/display/saver.h"
#include "infra/display/splash.h"

static Adafruit_GFX mock_gfx(128, 64);

void oled_begin() {}
void oled_flush() {}
Adafruit_GFX &oled_gfx() { return mock_gfx; }
bool display_present() { return true; }

void saver_note_boot() {}
bool saver_consume(bool) { return false; }
bool saver_showing() { return false; }
void saver_draw() {}

void splash_boot() {}
