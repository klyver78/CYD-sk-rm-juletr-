#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ESP32-2432S028R CYD, 320 x 240 landscape. Pinout/calibration follows CYD-test.
static constexpr int W = 320, H = 240;
static constexpr int TOUCH_IRQ = 36, TOUCH_MOSI = 32, TOUCH_MISO = 39;
static constexpr int TOUCH_CLK = 25, TOUCH_CS = 33;
static constexpr int TOUCH_MIN_X = 200, TOUCH_MAX_X = 3700;
static constexpr int TOUCH_MIN_Y = 240, TOUCH_MAX_Y = 3800;
static constexpr uint16_t BG = 0x0821;       // dyb marineblå
static constexpr uint16_t GREEN = 0x36A6;
static constexpr uint16_t GREEN_DARK = 0x1D23;
static constexpr uint16_t GREEN_LIGHT = 0x6F0A;
static constexpr uint16_t BROWN = 0x9A24;
static constexpr uint16_t OUTLINE = 0x2104;
static constexpr uint16_t RED = 0xF800, BLUE = 0x001F, YELLOW = 0xFFE0;
static constexpr uint8_t BACKLIGHT_PIN = 21;
static constexpr uint32_t DIM_AFTER_MS = 30000;
static constexpr uint32_t GREETING_MS = 3000;

TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

struct Bulb { int16_t x, y; uint16_t color; bool lit; uint32_t nextChange; };
Bulb bulbs[] = {
  {142, 84, BLUE, true, 0}, {181, 88, RED, true, 0},
  {119, 127, RED, true, 0}, {160, 132, BLUE, true, 0}, {202, 126, YELLOW, true, 0},
  {91, 171, BLUE, true, 0}, {137, 179, YELLOW, true, 0}, {184, 174, RED, true, 0}, {225, 164, BLUE, true, 0}
};
static constexpr size_t BULB_COUNT = sizeof(bulbs) / sizeof(bulbs[0]);
uint32_t bootAt = 0, greetingUntil = 0;
bool dimmed = false, greetingVisible = false;

uint16_t scale565(uint16_t c, uint8_t amount) {
  uint16_t r = ((c >> 11) & 31) * amount / 255;
  uint16_t g = ((c >> 5) & 63) * amount / 255;
  uint16_t b = (c & 31) * amount / 255;
  return (r << 11) | (g << 5) | b;
}

void drawBulb(size_t i) {
  const Bulb &b = bulbs[i];
  // Repaint only the bulb patch when its state changes.
  tft.fillCircle(b.x, b.y, 8, BG);
  if (b.lit) {
    tft.fillCircle(b.x, b.y, 7, scale565(b.color, 70));
    tft.fillCircle(b.x, b.y, 4, b.color);
    tft.fillCircle(b.x - 1, b.y - 2, 1, TFT_WHITE);
  } else {
    tft.fillCircle(b.x, b.y, 5, scale565(b.color, 55));
    tft.drawCircle(b.x, b.y, 6, OUTLINE);
  }
}

void drawStar(uint8_t brightness) {
  const uint16_t face = scale565(YELLOW, brightness);
  const uint16_t edge = scale565(0xFD20, brightness);
  const int cx=160, cy=24;
  tft.fillCircle(cx, cy, 17, BG);
  tft.fillCircle(cx, cy, 13, edge);
  tft.fillCircle(cx, cy, 9, face);
  tft.fillTriangle(cx, cy-20, cx-4, cy-5, cx+4, cy-5, edge);
  tft.fillTriangle(cx, cy+18, cx-4, cy+4, cx+4, cy+4, edge);
  tft.fillTriangle(cx-19, cy-6, cx-4, cy-3, cx-7, cy+3, edge);
  tft.fillTriangle(cx+19, cy-6, cx+4, cy-3, cx+7, cy+3, edge);
  tft.fillTriangle(cx-14, cy+13, cx-6, cy+2, cx-2, cy+7, edge);
  tft.fillTriangle(cx+14, cy+13, cx+6, cy+2, cx+2, cy+7, edge);
  tft.fillTriangle(cx, cy-14, cx-2, cy-4, cx+2, cy-4, face);
  tft.fillCircle(cx, cy, 5, face);
}

void drawScene() {
  tft.fillScreen(BG);
  // Ground shadow and trunk.
  tft.fillEllipse(160, 225, 92, 7, 0x18E3);
  tft.fillRoundRect(142, 188, 36, 43, 8, BROWN);
  tft.drawRoundRect(142, 188, 36, 43, 8, OUTLINE);
  // Tree silhouette, layered branches, and scalloped boughs.
  tft.fillTriangle(160, 34, 48, 202, 272, 202, OUTLINE);
  tft.fillTriangle(160, 42, 57, 196, 263, 196, GREEN_DARK);
  tft.fillTriangle(160, 44, 64, 190, 256, 190, GREEN);
  tft.fillTriangle(160, 60, 74, 151, 246, 151, GREEN_LIGHT);
  tft.fillTriangle(160, 90, 59, 178, 261, 178, GREEN);
  tft.fillTriangle(160, 119, 48, 207, 272, 207, GREEN_DARK);
  tft.fillTriangle(160, 35, 66, 164, 254, 164, GREEN);
  // Branch scallops give the stylized tree a soft silhouette.
  for (int x=77; x<=243; x+=22) {
    const int y = 82 + (x-160)*(x-160)/230;
    tft.fillCircle(x, y, 13, GREEN_LIGHT);
    tft.fillCircle(x, y+27, 15, GREEN);
    tft.fillCircle(x, y+54, 17, GREEN_DARK);
    tft.fillCircle(x, y+81, 19, GREEN);
  }
  // Dark garland strands, drawn before the lights.
  tft.drawArc(160, 91, 60, 23, 10, 170, OUTLINE, BG);
  tft.drawArc(160, 132, 88, 25, 10, 170, OUTLINE, BG);
  tft.drawArc(160, 174, 108, 28, 10, 170, OUTLINE, BG);
  drawStar(255);
  for (size_t i=0; i<BULB_COUNT; ++i) drawBulb(i);
}

void drawGreeting() {
  // Heart and lettering occupy a single overlay; the tree is restored on timeout.
  const int cx=160, cy=117;
  tft.fillCircle(cx-19, cy-12, 29, RED);
  tft.fillCircle(cx+19, cy-12, 29, RED);
  tft.fillTriangle(cx-47, cy-4, cx+47, cy-4, cx, cy+53, RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, RED);
  tft.drawString("Klyver", cx, cy-9, 4);
  tft.drawString("JUL", cx, cy+20, 4);
}

bool readTap() {
  static bool wasDown=false;
  bool down = touch.tirqTouched() && touch.touched();
  if (!down) { wasDown=false; return false; }
  if (wasDown) return false;
  wasDown=true;
  delay(12);
  TS_Point p=touch.getPoint();
  return p.z >= 200;
}

uint8_t starBrightness(uint32_t now) {
  // Smooth, slow pulse while preserving the star's yellow hue.
  return (uint8_t)(150 + (sin(now / 900.0f) + 1.0f) * 52.0f);
}

void setup() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, 255);
  tft.init();
  tft.setRotation(1);
  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
  randomSeed(esp_random());
  bootAt=millis();
  for (size_t i=0; i<BULB_COUNT; ++i) bulbs[i].nextChange=bootAt+random(2000,6001);
  drawScene();
}

void loop() {
  const uint32_t now=millis();
  if (readTap()) {
    if (dimmed) {
      dimmed=false;
      analogWrite(BACKLIGHT_PIN, 255);
      bootAt=now; // restart the 30-second idle timer
    } else {
      greetingVisible=true;
      greetingUntil=now+GREETING_MS;
      drawGreeting();
    }
  }

  if (!dimmed && !greetingVisible && now-bootAt >= DIM_AFTER_MS) {
    dimmed=true;
    analogWrite(BACKLIGHT_PIN, 77); // cirka 30 %
  }

  if (greetingVisible && (int32_t)(now-greetingUntil) >= 0) {
    greetingVisible=false;
    drawScene();
    // Re-draw only the current lamp patches over the restored static illustration.
    for (size_t i=0; i<BULB_COUNT; ++i) drawBulb(i);
  }

  if (!greetingVisible) {
    for (size_t i=0; i<BULB_COUNT; ++i) {
      if ((int32_t)(now-bulbs[i].nextChange) >= 0) {
        bulbs[i].lit=!bulbs[i].lit;
        bulbs[i].nextChange=now+random(2000,6001);
        drawBulb(i);
      }
    }
    static uint32_t lastStar=0;
    if (now-lastStar >= 100) {
      drawStar(starBrightness(now));
      lastStar=now;
    }
  }
  delay(10);
}
