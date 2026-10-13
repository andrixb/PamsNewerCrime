#include "infra/display/splash.h"
#include "infra/display/oled.h"

// ---------------------------------------------------------------------------
// Boot splash: "Pam's Newer Crime" title + a stick-figure heist chase. Pam
// (ponytail, dress, loot sack) sprints right across the panel with a cop in
// hot pursuit, over a scrolling sidewalk, while a progress bar fills. Runs
// from ui_init(), before ctrl_timer is armed, so delay()/SPI here are safe.
// ---------------------------------------------------------------------------
#if PNC_SPLASH
// Leg swing per animation frame: front foot / back foot x-offset from the hip,
// and the mid-stride knee lift (foot rises 2px on the swing-through frame).
static const int8_t LEG_F[4] = { -5, -1, 5, 1 };
static const int8_t LEG_B[4] = { 5, 1, -5, -1 };

static void draw_legs(int16_t hip_x, int16_t hip_y, int16_t ground_y, uint8_t ph) {
  Adafruit_GFX &gfx = oled_gfx();
  int16_t f = ground_y - ((ph == 1 || ph == 3) ? 2 : 0);
  int16_t b = ground_y - ((ph == 0 || ph == 2) ? 2 : 0);
  gfx.drawLine(hip_x, hip_y, hip_x + LEG_F[ph], f, PNC_WHITE);
  gfx.drawLine(hip_x, hip_y, hip_x + LEG_B[ph], b, PNC_WHITE);
}

static void draw_pam(int16_t x, int16_t by, uint8_t ph) {
  Adafruit_GFX &gfx = oled_gfx();
  // Head with flowing ponytail (whips with the stride)
  gfx.fillCircle(x + 3, by - 18, 3, PNC_WHITE);
  gfx.drawLine(x, by - 19, x - 5, by - 17 + (ph & 1), PNC_WHITE);
  gfx.drawLine(x, by - 17, x - 6, by - 14 + (ph == 2), PNC_WHITE);
  // Shoulder to A-line dress, leaning into the run
  gfx.drawLine(x + 3, by - 15, x + 1, by - 10, PNC_WHITE);
  gfx.fillTriangle(x + 2, by - 13, x - 3, by - 6, x + 6, by - 6, PNC_WHITE);
  // Arms pump in counterphase; the back arm trails the loot sack
  gfx.drawLine(x + 2, by - 13, x + 9, by - 15 - (ph & 1), PNC_WHITE);
  gfx.drawLine(x + 1, by - 13, x - 6, by - 10 + (ph & 1), PNC_WHITE);
  gfx.fillCircle(x - 8, by - 9 + (ph & 1), 2, PNC_WHITE);
  gfx.drawPixel(x - 7, by - 12 + (ph & 1), PNC_WHITE);  // sack knot
  draw_legs(x + 1, by - 6, by, ph);
}

static void draw_cop(int16_t x, int16_t by, uint8_t ph) {
  Adafruit_GFX &gfx = oled_gfx();
  // Head under a flat cap with brim
  gfx.fillCircle(x + 3, by - 18, 3, PNC_WHITE);
  gfx.fillRect(x - 1, by - 22, 7, 3, PNC_WHITE);
  gfx.drawLine(x + 5, by - 20, x + 9, by - 20, PNC_WHITE);
  // Stocky torso, badge glint
  gfx.fillRect(x + 1, by - 15, 3, 9, PNC_WHITE);
  gfx.drawPixel(x + 4, by - 13, PNC_WHITE);
  // Both arms outstretched toward Pam, reaching with each stride
  int16_t reach = (ph & 1) ? 0 : 2;
  gfx.drawLine(x + 3, by - 14, x + 11 + reach, by - 16, PNC_WHITE);
  gfx.drawLine(x + 3, by - 12, x + 11 + reach, by - 12, PNC_WHITE);
  draw_legs(x + 2, by - 6, by, ph ^ 2);  // half a cycle out of phase
}

static void ui_splash() {
  Adafruit_GFX &gfx = oled_gfx();
  const uint32_t run_ms = 3200;
  const int16_t ground_y = 56, ground_line = 58;
  uint32_t t0 = millis();
  Serial.println("pnc: boot splash");
  while (true) {
    uint32_t el = millis() - t0;
    if (el >= run_ms) break;
    gfx.fillScreen(PNC_BLACK);
    gfx.setTextColor(PNC_WHITE);
    gfx.setTextSize(1);
    // Title card
    gfx.setCursor((128 - 17 * 6) / 2, 0);
    gfx.printf("PAM'S NEWER CRIME");
    gfx.drawLine(0, 9, 127, 9, PNC_WHITE);
    // Slim loader strip at the top; the chase keeps the whole street below it
    gfx.drawRect(8, 12, 112, 6, PNC_WHITE);
    gfx.fillRect(10, 14, (int16_t)((int32_t)el * 108 / run_ms), 2, PNC_WHITE);
    // Street: solid curb + moving sidewalk cracks for parallax
    gfx.drawLine(0, ground_line, 127, ground_line, PNC_WHITE);
    for (int16_t x = -((int16_t)(el / 20) % 16); x < 128; x += 16)
      gfx.drawLine(x, 62, x + 7, 62, PNC_WHITE);
    uint8_t ph = (uint8_t)((el / 120) & 3);
    int16_t bob = (ph == 1 || ph == 3) ? 1 : 0;  // whole-body bounce mid-stride
    // Speed lines crackling behind the cop as the strides land
    if (ph == 0 || ph == 2) {
      gfx.drawLine(14, 42, 26, 42, PNC_WHITE);
      gfx.drawLine(10, 50, 20, 50, PNC_WHITE);
    } else {
      gfx.drawLine(12, 46, 24, 46, PNC_WHITE);
    }
    draw_cop(48, ground_y - bob, ph);
    draw_pam(88, ground_y - bob, ph);
    oled_flush();
    delay(35);
  }
  gfx.fillScreen(PNC_BLACK);
  oled_flush();
}
#endif  // PNC_SPLASH

void splash_boot() {
  Adafruit_GFX &gfx = oled_gfx();
  gfx.setTextSize(1);
  gfx.cp437(true);
#if PNC_SPLASH
  ui_splash();
  gfx.setTextColor(PNC_WHITE);
#endif
}
