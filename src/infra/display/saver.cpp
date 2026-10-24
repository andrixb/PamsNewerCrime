#include "infra/display/saver.h"
#include "infra/display/oled.h"
#include "infra/hal_input.h"

static uint32_t last_activity_ms = 0;
static bool saver_active = false;
static bool saver_wake_hold = false;
static uint32_t saver_t0 = 0;

void saver_note_boot() { last_activity_ms = millis(); }

bool saver_consume(bool busy) {
#if PNC_SA_TIMEOUT_MS
  // Idle tracking + screensaver gate. While the saver is up, the first input
  // only wakes it; that same turn/press is swallowed by saver_wake_hold below
  // so it can never nudge a parameter.
  bool activity = enc_l.moved || enc_r.moved ||
                  btn_enc_sw.just_pressed() || btn_enc_r_sw.just_pressed() ||
                  btn_b1.just_pressed() || btn_b2.just_pressed();
  if (activity) last_activity_ms = millis();
  if (saver_active) {
    if (activity) { saver_active = false; saver_wake_hold = true; }
    return true;
  }
  if (saver_wake_hold) {
    enc_l.read(); enc_r.read();  // discard the wake rotation
    if (!activity && !btn_enc_sw.pressed() && !btn_enc_r_sw.pressed() &&
        !btn_b1.pressed() && !btn_b2.pressed())
      saver_wake_hold = false;
    return true;
  }
  // Enter the saver only from a plain browsing page: never mid-edit of a scale
  // or ratio, or while the app menu is open (those states own the encoders).
  if (!busy && millis() - last_activity_ms >= PNC_SA_TIMEOUT_MS) {
    saver_active = true;
    saver_t0 = millis();
    return true;
  }
  return false;
#else
  (void)busy;
  return false;
#endif
}

bool saver_showing() {
#if PNC_SA_TIMEOUT_MS
  return saver_active;
#else
  return false;
#endif
}

void saver_draw() {
#if PNC_SA_TIMEOUT_MS
  Adafruit_GFX &gfx = oled_gfx();
  float s = (millis() - saver_t0) * 0.00035f;
  // Two sources drift around the panel, out of step, so the moire never repeats
  // and no pixel stays lit long (OLED-friendly).
  float sx1 = 64.0f + 34.0f * sinf(s * 1.3f);
  float sy1 = 32.0f + 20.0f * cosf(s * 1.0f);
  float sx2 = 64.0f + 34.0f * sinf(s + 2.1f);
  float sy2 = 32.0f + 20.0f * cosf(s * 1.7f + 1.1f);
  float k1 = 0.62f + 0.10f * sinf(s * 0.5f);   // ring density, breathing
  float k2 = 0.50f + 0.10f * cosf(s * 0.7f);
  float p1 = s * 3.0f, p2 = -s * 2.4f;         // outward-travelling crests
  gfx.fillScreen(PNC_BLACK);
  for (int16_t y = 0; y < 64; y++) {
    float dy1 = y - sy1, dy2 = y - sy2;
    for (int16_t x = 0; x < 128; x++) {
      float dx1 = x - sx1, dx2 = x - sx2;
      float v = sinf(k1 * sqrtf(dx1 * dx1 + dy1 * dy1) + p1) +
                sinf(k2 * sqrtf(dx2 * dx2 + dy2 * dy2) + p2);
      if (v > 0.9f) gfx.drawPixel(x, y, PNC_WHITE);
    }
  }
  // Corner tag so the panel edge reads even on a dark cycle.
  gfx.setTextColor(PNC_WHITE);
  gfx.setCursor(0, 56);
  gfx.printf("idle");
  oled_flush();
#endif
}
