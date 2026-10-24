#include "infra/display/oled.h"
#include <SPI.h>
#include <string.h>

class PncSpiOled : public Adafruit_GFX {
public:
  PncSpiOled() : Adafruit_GFX(128, 64) {
    memset(fb, 0, sizeof(fb));
    memset(last, 0xFF, sizeof(last));
  }
  void begin() {
    pinMode(PIN_OLED_CS, OUTPUT);  digitalWrite(PIN_OLED_CS, HIGH);
    pinMode(PIN_OLED_DC, OUTPUT);  digitalWrite(PIN_OLED_DC, HIGH);
    pinMode(PIN_OLED_RST, OUTPUT);
    digitalWrite(PIN_OLED_RST, HIGH); delay(1);
    digitalWrite(PIN_OLED_RST, LOW);  delay(10);
    digitalWrite(PIN_OLED_RST, HIGH); delay(20);
    static const uint8_t init_seq[] = {
      0xAE,             // display off
      0xD5, 0x80,       // clock divide / osc freq
      0xA8, 0x3F,       // multiplex 1/64
      0xD3, 0x00,       // display offset 0
      0x40,             // start line 0
      0x8D, 0x14,       // charge pump
      0x20, 0x00,       // page addressing mode
      0xA1, 0xC8,       // segment remap / scan dir
      0xDA, 0x12,       // com pins config
      0x81, 0xCF,       // contrast
      0xD9, 0xF1,       // pre-charge period
      0xDB, 0x40,       // vcomh deselect
      0x2E,             // no scroll
      0xA4,             // output RAM->panel
      0xA6,             // non-inverted
      0xAF,             // display on
    };
    digitalWrite(PIN_OLED_CS, LOW);
    xfer(false, init_seq, sizeof(init_seq));
    digitalWrite(PIN_OLED_CS, HIGH);
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
    uint8_t *p = &fb[(y >> 3) * 128 + x];
    if (c) *p |=  (uint8_t)(1u << (y & 7));
    else   *p &= (uint8_t)~(1u << (y & 7));
  }
  void flush() {
    for (uint8_t pg = 0; pg < 8; pg++) {
      uint8_t *p = &fb[pg * 128];
      uint8_t *q = &last[pg * 128];
      if (memcmp(p, q, 128) == 0) continue;
      memcpy(q, p, 128);
      const uint8_t seq[3] = { 0x10, 0x02, (uint8_t)(0xB0 | pg) };
      noInterrupts();
      digitalWrite(PIN_OLED_CS, LOW);
      xfer(false, seq, 3);          // command phase (DC low), CS held
      xfer(true, p, 128);           // data phase (DC high)
      digitalWrite(PIN_OLED_CS, HIGH);
      interrupts();
      delayMicroseconds(2);
    }
  }
private:
  uint8_t fb[1024];
  uint8_t last[1024];
  // The OLED shares the bus with the DAC8565, which runs transactions at
  // 12 MHz. SH1106 panels glitch above ~4-8 MHz (corrupted bytes = random
  // dots/lines), so every OLED burst must carry its own SPISettings instead
  // of inheriting whatever clock the DAC left on the peripheral.
  void xfer(bool data, const uint8_t *b, size_t n) {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_OLED_DC, data ? HIGH : LOW);
    for (size_t i = 0; i < n; i++) SPI.transfer(b[i]);
    SPI.endTransaction();
  }
};

static PncSpiOled oled_spi;
static bool display_ok = false;

void oled_begin() {
  oled_spi.begin();
  display_ok = true;
}

void oled_flush() { oled_spi.flush(); }

Adafruit_GFX &oled_gfx() { return oled_spi; }

bool display_present() { return display_ok; }
