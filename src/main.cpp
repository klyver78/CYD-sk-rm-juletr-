#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

// ESP32-2432S028R CYD, 320 x 240 landscape. Pinout/calibration follows CYD-test.
static constexpr int W = 320, H = 240;
static constexpr int TOUCH_IRQ = 36, TOUCH_MOSI = 32, TOUCH_MISO = 39;
static constexpr int TOUCH_CLK = 25, TOUCH_CS = 33;
static constexpr int TOUCH_MIN_X = 200, TOUCH_MAX_X = 3700;
static constexpr int TOUCH_MIN_Y = 240, TOUCH_MAX_Y = 3800;
static constexpr uint16_t BG = 0x0821;
static constexpr uint16_t GREEN = 0x36A6, GREEN_DARK = 0x1D23, GREEN_LIGHT = 0x6F0A;
static constexpr uint16_t BROWN = 0x9A24, OUTLINE = 0x2104;
static constexpr uint16_t RED = 0xF800, BLUE = 0x001F, YELLOW = 0xFFE0;
static constexpr uint8_t BACKLIGHT_PIN = 21;
static constexpr uint32_t DIM_AFTER_MS = 30000, GREETING_MS = 3000;
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

struct Bulb { int16_t x, y; uint16_t color; bool lit; uint32_t nextChange; };
// 50 individual lamps, distributed in seven rows across the layered branches.
Bulb bulbs[] = {
  {147,72,RED,true,0},{160,72,BLUE,true,0},{173,72,YELLOW,true,0},
  {128,91,YELLOW,true,0},{144,91,RED,true,0},{160,91,BLUE,true,0},{176,91,YELLOW,true,0},{192,91,RED,true,0},
  {111,110,BLUE,true,0},{127,110,YELLOW,true,0},{143,110,RED,true,0},{159,110,BLUE,true,0},{175,110,YELLOW,true,0},{191,110,RED,true,0},{207,110,BLUE,true,0},
  {101,129,RED,true,0},{120,129,BLUE,true,0},{140,129,YELLOW,true,0},{160,129,RED,true,0},{180,129,BLUE,true,0},{200,129,YELLOW,true,0},{219,129,RED,true,0},
  {89,148,BLUE,true,0},{109,148,YELLOW,true,0},{129,148,RED,true,0},{149,148,BLUE,true,0},{169,148,YELLOW,true,0},{189,148,RED,true,0},{209,148,BLUE,true,0},{229,148,YELLOW,true,0},
  {79,167,YELLOW,true,0},{98,167,RED,true,0},{118,167,BLUE,true,0},{138,167,YELLOW,true,0},{158,167,RED,true,0},{178,167,BLUE,true,0},{198,167,YELLOW,true,0},{218,167,RED,true,0},{237,167,BLUE,true,0},
  {70,186,RED,true,0},{87,186,BLUE,true,0},{104,186,YELLOW,true,0},{121,186,RED,true,0},{138,186,BLUE,true,0},{155,186,YELLOW,true,0},{172,186,RED,true,0},{189,186,BLUE,true,0},{206,186,YELLOW,true,0},{223,186,RED,true,0},{240,186,BLUE,true,0}
};
static constexpr size_t BULB_COUNT = sizeof(bulbs)/sizeof(bulbs[0]);
uint32_t bootAt=0, greetingUntil=0;
bool dimmed=false, greetingVisible=false;

uint16_t scale565(uint16_t c,uint8_t a) {
  uint16_t r=((c>>11)&31)*a/255, g=((c>>5)&63)*a/255, b=(c&31)*a/255;
  return (r<<11)|(g<<5)|b;
}

void drawBulb(size_t i) {
  const Bulb &b=bulbs[i];
  // Keep animation local to each lamp so the static tree never flickers.
  tft.fillCircle(b.x,b.y,6,GREEN);
  tft.drawCircle(b.x,b.y,6,OUTLINE);
  if(b.lit) {
    tft.fillCircle(b.x,b.y,5,scale565(b.color,90));
    tft.fillCircle(b.x,b.y,3,b.color);
    tft.fillCircle(b.x-1,b.y-2,1,TFT_WHITE);
  } else {
    tft.fillCircle(b.x,b.y,3,scale565(b.color,65));
  }
}

void drawStar(uint8_t brightness) {
  const uint16_t face=scale565(YELLOW,brightness), edge=scale565(0xFD20,brightness);
  const int cx=160,cy=25;
  tft.fillRect(cx-21,cy-22,42,44,BG);
  tft.fillCircle(cx,cy,13,OUTLINE);
  tft.fillTriangle(cx,cy-22,cx-5,cy-4,cx+5,cy-4,edge);
  tft.fillTriangle(cx,cy+21,cx-6,cy+3,cx+6,cy+3,edge);
  tft.fillTriangle(cx-20,cy-7,cx-4,cy-4,cx-7,cy+4,edge);
  tft.fillTriangle(cx+20,cy-7,cx+4,cy-4,cx+7,cy+4,edge);
  tft.fillTriangle(cx-14,cy+15,cx-5,cy+1,cx-2,cy+8,edge);
  tft.fillTriangle(cx+14,cy+15,cx+5,cy+1,cx+2,cy+8,edge);
  tft.fillCircle(cx,cy,7,face);
  tft.fillCircle(cx,cy-2,3,0xFFEF);
}

// A bough is a broad, dark-edged skirt with rounded, jagged branch tips.
void drawBough(int16_t tipY,int16_t leftX,int16_t baseY,int16_t rightX,
               uint16_t leaf,size_t lobes) {
  tft.fillTriangle(160,tipY,leftX,baseY-5,rightX,baseY-5,OUTLINE);
  tft.fillTriangle(160,tipY+5,leftX+7,baseY-10,rightX-7,baseY-10,leaf);
  const int16_t span=rightX-leftX;
  for(size_t n=0;n<lobes;++n) {
    int16_t x=leftX+8+(int32_t)(span-16)*n/(lobes-1);
    int16_t r=(span>190)?16:((span>130)?14:11);
    tft.fillCircle(x,baseY-7,r,OUTLINE);
    tft.fillCircle(x,baseY-10,r-4,leaf);
  }
  // Short side boughs create the stepped, feathery silhouette from the reference.
  for(int side=-1;side<=1;side+=2) {
    int16_t y=tipY+(baseY-tipY)*2/3;
    int16_t x=160+side*(rightX-160)*2/3;
    tft.fillTriangle(x,y-7,x+side*17,y+4,x-side*3,y+8,OUTLINE);
    tft.fillTriangle(x,y-7,x+side*13,y+1,x-side*2,y+5,leaf);
  }
}

void drawScene() {
  tft.fillScreen(BG);
  // Shadow and trunk.
  tft.fillEllipse(160,228,89,7,0x18E3);
  tft.fillRoundRect(143,185,34,46,8,BROWN);
  tft.drawRoundRect(143,185,34,46,8,OUTLINE);
  tft.fillTriangle(146,193,160,200,174,193,0xC348);

  // Five overlapping, flared evergreen skirts, drawn from the bottom upwards.
  drawBough(133,30,203,290,GREEN_DARK,11);
  drawBough(114,48,180,272,GREEN,9);
  drawBough(94,67,157,253,GREEN_DARK,8);
  drawBough(74,91,133,229,GREEN,7);
  drawBough(52,118,107,202,GREEN_DARK,5);
  drawBough(43,146,82,174,GREEN,3);

  // Layered highlights on the foliage.
  tft.fillTriangle(160,54,128,101,149,98,0x4E27);
  tft.fillTriangle(160,78,100,145,137,137,0x4E27);
  tft.fillTriangle(160,105,76,170,119,163,0x4E27);
  tft.fillTriangle(160,130,48,193,91,187,0x4E27);
  // Small gold garland dots and candy decorations echo the supplied illustration.
  for(int x=96;x<=224;x+=8) tft.fillCircle(x,145+(x%3),1,0xFFE0);
  for(int x=75;x<=245;x+=10) tft.fillCircle(x,174+(x%4),1,0xFFE0);
  tft.fillCircle(116,121,4,0x07FF); tft.fillCircle(204,121,4,0xFD20);
  tft.fillCircle(91,156,4,0xFBE0); tft.fillCircle(229,156,4,0x07FF);

  drawStar(255);
  for(size_t i=0;i<BULB_COUNT;++i) drawBulb(i);
}

void drawGreeting() {
  const int cx=160,cy=117;
  tft.fillCircle(cx-19,cy-12,29,RED);
  tft.fillCircle(cx+19,cy-12,29,RED);
  tft.fillTriangle(cx-47,cy-4,cx+47,cy-4,cx,cy+53,RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN,RED);
  tft.drawString("Klyver",cx,cy-9,4);
  tft.drawString("JUL",cx,cy+20,4);
}
bool readTap() {
  static bool wasDown=false;
  bool down=touch.tirqTouched()&&touch.touched();
  if(!down){wasDown=false;return false;}
  if(wasDown)return false;
  wasDown=true;delay(12);
  TS_Point p=touch.getPoint();
  return p.z>=200;
}
uint8_t starBrightness(uint32_t now){return(uint8_t)(150+(sin(now/900.0f)+1.0f)*52.0f);}

void setup() {
  pinMode(BACKLIGHT_PIN,OUTPUT);analogWrite(BACKLIGHT_PIN,255);
  tft.init();tft.setRotation(1);
  touchSPI.begin(TOUCH_CLK,TOUCH_MISO,TOUCH_MOSI,TOUCH_CS);
  touch.begin(touchSPI);touch.setRotation(1);
  randomSeed(esp_random());bootAt=millis();
  for(size_t i=0;i<BULB_COUNT;++i)bulbs[i].nextChange=bootAt+random(2000,6001);
  drawScene();
}
void loop() {
  const uint32_t now=millis();
  if(readTap()) {
    if(dimmed){dimmed=false;analogWrite(BACKLIGHT_PIN,255);bootAt=now;}
    else{greetingVisible=true;greetingUntil=now+GREETING_MS;drawGreeting();}
  }
  if(!dimmed&&!greetingVisible&&now-bootAt>=DIM_AFTER_MS){dimmed=true;analogWrite(BACKLIGHT_PIN,77);}
  if(greetingVisible&&(int32_t)(now-greetingUntil)>=0) {
    greetingVisible=false;drawScene();
  }
  if(!greetingVisible) {
    for(size_t i=0;i<BULB_COUNT;++i) if((int32_t)(now-bulbs[i].nextChange)>=0) {
      bulbs[i].lit=!bulbs[i].lit;bulbs[i].nextChange=now+random(2000,6001);drawBulb(i);
    }
    static uint32_t lastStar=0;
    if(now-lastStar>=100){drawStar(starBrightness(now));lastStar=now;}
  }
  delay(10);
}
