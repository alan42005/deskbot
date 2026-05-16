/*
  EMO Bot - Full Sensor Firmware
  Hardware: ESP32 + ILI9341 TFT + DHT11 + Buzzer + DS3231 (I2C)
  BLE UUIDs:
    Service:  12345678-1234-1234-1234-123456789abc
    AnimChar: ...9001  write 1 byte = animation index 0-7
    TextChar: ...9002  write string = speech bubble
    SensChar: ...9003  notify "T:25 H:60" sensor data
    BuzzChar: ...9004  write 1 byte = tone pattern (0-5)
*/

#include <SPI.h>
#include <Wire.h>
#include <DHT.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- BLE UUIDs ---
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define ANIM_CHAR_UUID      "12345678-1234-1234-1234-123456789001"
#define TEXT_CHAR_UUID      "12345678-1234-1234-1234-123456789002"
#define SENS_CHAR_UUID      "12345678-1234-1234-1234-123456789003"
#define BUZZ_CHAR_UUID      "12345678-1234-1234-1234-123456789004"

// --- PIN MAPPING ---
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI  23
#define TFT_CLK   18
#define TFT_MISO  19
#define TFT_LED   32   // PWM backlight
#define TOUCH_PIN 13   // T4 capacitive

// --- NEW SENSOR PINS ---
#define DHT_PIN    33  // DHT11 data pin
#define BUZZER_PIN 25  // Passive buzzer
#define I2C_SDA    26  // Software I2C SDA → DS3231
#define I2C_SCL    27  // Software I2C SCL → DS3231
#define DHT_TYPE   DHT11
#define LEDC_BUZZ_CH  0  // LEDC channel for buzzer
#define LEDC_BL_CH    1  // LEDC channel for backlight

// --- COLOR CONSTANTS (RGB565) ---
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_DARKRED   0xB000
#define COLOR_EYE_GREY  0xDEFB
#define COLOR_CYAN      0x07FF
#define COLOR_TEAR      0x041F
#define COLOR_PINK      0xF81F
#define COLOR_DARKGRAY  0x7BEF
#define COLOR_BROWN     0xC220

// --- FORWARD DECLARATIONS ---
void fillScreen(uint16_t color);
void renderCurrentFrame();
void showMessage(const String& txt);
void playTone(int pattern);

// --- ANIMATION STATE ---
int currentAnimation = 1;  // Default to Looking
int currentFrame = 0;
unsigned long lastFrameTime = 0;
unsigned long lastTouchTime = 0;
const int maxFrames[] = {5, 5, 4, 4, 5, 5, 5, 4};
int  userAnimation  = 1;   // Default to Looking
bool autoMoodActive = false;
unsigned long touchOverrideEnd = 0;

bool bleConnected    = false;
bool bleWasConnected = false;

// --- SENSOR STATE ---
DHT dht(DHT_PIN, DHT_TYPE);
BLECharacteristic* sensChar = nullptr;

float  lastTemp = 25.0, lastHumi = 50.0;
unsigned long lastSensorRead = 0;

// --- FEATURE STATES ---
bool pomoActive = false;
unsigned long pomoEndTime = 0;
bool alarmActive = false;
int alarmHour = 0;
int alarmMin = 0;
bool weatherModeActive = false;

// ==========================================
// BLE CALLBACKS
// ==========================================

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* srv) override    { bleConnected = true; }
  void onDisconnect(BLEServer* srv) override { bleConnected = false; }
};

class AnimCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    auto val = c->getData();
    if (c->getLength() >= 1) {
      int idx = val[0];
      if (idx >= 0 && idx < 8) {
        currentAnimation = idx;
        currentFrame = 0;
        lastFrameTime = millis();
        fillScreen(COLOR_BLACK);
        renderCurrentFrame();
      }
    }
  }
};

class TextCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String txt = c->getValue().c_str();
    if (txt.startsWith("CMD:POMO:")) {
      int mins = txt.substring(9).toInt();
      pomoActive = true;
      pomoEndTime = millis() + mins * 60000UL;
      userAnimation = 1; // Looking / Focus
      currentAnimation = 1;
      weatherModeActive = false;
      fillScreen(COLOR_BLACK); renderCurrentFrame();
      playTone(1);
    } else if (txt.startsWith("CMD:ALRM:")) {
      alarmHour = txt.substring(9, 11).toInt();
      alarmMin = txt.substring(12, 14).toInt();
      alarmActive = true;
      playTone(1);
      showMessage("ALARM SET");
    } else if (txt.startsWith("CMD:NOTI:")) {
      playTone(1); // ding
      showMessage("NEW MSG!");
    } else if (txt.length() > 0) {
      showMessage(txt);
    }
  }
};

class BuzzCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    if (c->getLength() >= 1) playTone(c->getData()[0]);
  }
};

// ==========================================
// 5x7 BITMAP FONT (ASCII 32-90)
// Each char = 5 column bytes, bit0=top row
// ==========================================
const uint8_t font5x7[][5] PROGMEM = {
  {0x00,0x00,0x00,0x00,0x00}, // ' ' 32
  {0x00,0x00,0x5F,0x00,0x00}, // '!' 33
  {0x00,0x07,0x00,0x07,0x00}, // '"' 34
  {0x14,0x7F,0x14,0x7F,0x14}, // '#' 35
  {0x24,0x2A,0x7F,0x2A,0x12}, // '$' 36
  {0x23,0x13,0x08,0x64,0x62}, // '%' 37
  {0x36,0x49,0x55,0x22,0x50}, // '&' 38
  {0x00,0x05,0x03,0x00,0x00}, // '\'' 39
  {0x00,0x1C,0x22,0x41,0x00}, // '(' 40
  {0x00,0x41,0x22,0x1C,0x00}, // ')' 41
  {0x08,0x2A,0x1C,0x2A,0x08}, // '*' 42
  {0x08,0x08,0x3E,0x08,0x08}, // '+' 43
  {0x00,0x50,0x30,0x00,0x00}, // ',' 44
  {0x08,0x08,0x08,0x08,0x08}, // '-' 45
  {0x00,0x60,0x60,0x00,0x00}, // '.' 46
  {0x20,0x10,0x08,0x04,0x02}, // '/' 47
  {0x3E,0x51,0x49,0x45,0x3E}, // '0' 48
  {0x00,0x42,0x7F,0x40,0x00}, // '1' 49
  {0x42,0x61,0x51,0x49,0x46}, // '2' 50
  {0x21,0x41,0x45,0x4B,0x31}, // '3' 51
  {0x18,0x14,0x12,0x7F,0x10}, // '4' 52
  {0x27,0x45,0x45,0x45,0x39}, // '5' 53
  {0x3C,0x4A,0x49,0x49,0x30}, // '6' 54
  {0x01,0x71,0x09,0x05,0x03}, // '7' 55
  {0x36,0x49,0x49,0x49,0x36}, // '8' 56
  {0x06,0x49,0x49,0x29,0x1E}, // '9' 57
  {0x00,0x36,0x36,0x00,0x00}, // ':' 58
  {0x00,0x56,0x36,0x00,0x00}, // ';' 59
  {0x00,0x08,0x14,0x22,0x41}, // '<' 60
  {0x14,0x14,0x14,0x14,0x14}, // '=' 61
  {0x41,0x22,0x14,0x08,0x00}, // '>' 62
  {0x02,0x01,0x51,0x09,0x06}, // '?' 63
  {0x32,0x49,0x79,0x41,0x3E}, // '@' 64
  {0x7E,0x11,0x11,0x11,0x7E}, // 'A' 65
  {0x7F,0x49,0x49,0x49,0x36}, // 'B' 66
  {0x3E,0x41,0x41,0x41,0x22}, // 'C' 67
  {0x7F,0x41,0x41,0x22,0x1C}, // 'D' 68
  {0x7F,0x49,0x49,0x49,0x41}, // 'E' 69
  {0x7F,0x09,0x09,0x09,0x01}, // 'F' 70
  {0x3E,0x41,0x49,0x49,0x7A}, // 'G' 71
  {0x7F,0x08,0x08,0x08,0x7F}, // 'H' 72
  {0x00,0x41,0x7F,0x41,0x00}, // 'I' 73
  {0x20,0x40,0x41,0x3F,0x01}, // 'J' 74
  {0x7F,0x08,0x14,0x22,0x41}, // 'K' 75
  {0x7F,0x40,0x40,0x40,0x40}, // 'L' 76
  {0x7F,0x02,0x0C,0x02,0x7F}, // 'M' 77
  {0x7F,0x04,0x08,0x10,0x7F}, // 'N' 78
  {0x3E,0x41,0x41,0x41,0x3E}, // 'O' 79
  {0x7F,0x09,0x09,0x09,0x06}, // 'P' 80
  {0x3E,0x41,0x51,0x21,0x5E}, // 'Q' 81
  {0x7F,0x09,0x19,0x29,0x46}, // 'R' 82
  {0x46,0x49,0x49,0x49,0x31}, // 'S' 83
  {0x01,0x01,0x7F,0x01,0x01}, // 'T' 84
  {0x3F,0x40,0x40,0x40,0x3F}, // 'U' 85
  {0x1F,0x20,0x40,0x20,0x1F}, // 'V' 86
  {0x3F,0x40,0x38,0x40,0x3F}, // 'W' 87
  {0x63,0x14,0x08,0x14,0x63}, // 'X' 88
  {0x07,0x08,0x70,0x08,0x07}, // 'Y' 89
  {0x61,0x51,0x49,0x45,0x43}, // 'Z' 90
};

// ==========================================
// TFT FUNCTIONS (same as original)
// ==========================================

void writeCmd(uint8_t cmd) {
  digitalWrite(TFT_DC, LOW); digitalWrite(TFT_CS, LOW);
  SPI.write(cmd); digitalWrite(TFT_CS, HIGH);
}
void writeData(uint8_t data) {
  digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  SPI.write(data); digitalWrite(TFT_CS, HIGH);
}
void writeData16(uint16_t data) {
  digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  SPI.write16(data); digitalWrite(TFT_CS, HIGH);
}
void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  writeCmd(0x2A); writeData16(x0); writeData16(x1);
  writeCmd(0x2B); writeData16(y0); writeData16(y1);
  writeCmd(0x2C);
}
void fillRect(int x, int y, int w, int h, uint16_t color) {
  if (x < 0 || y < 0 || x + w > 320 || y + h > 240) return;
  setWindow(x, y, x + w - 1, y + h - 1);
  digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  for (uint32_t i = 0; i < (uint32_t)w * h; i++) SPI.write16(color);
  digitalWrite(TFT_CS, HIGH);
}
void fillScreen(uint16_t color) { fillRect(0, 0, 320, 240, color); }
void fillCircle(int x0, int y0, int r, uint16_t color) {
  for (int y = -r; y <= r; y++) {
    int w = sqrt(r * r - y * y);
    fillRect(x0 - w, y0 + y, w * 2, 1, color);
  }
}
void fillOval(int cx, int cy, int rx, int ry, uint16_t color) {
  for (int y = -ry; y <= ry; y++) {
    float dy = (float)y / ry;
    int w = rx * sqrt(1.0 - dy * dy);
    fillRect(cx - w, cy + y, w * 2, 1, color);
  }
}

// ==========================================
// TEXT RENDERING (5x7 font, scaled)
// ==========================================

// Draw one character at (x,y), scale = pixel size per font pixel
void drawChar(int x, int y, char ch, uint16_t color, uint8_t scale) {
  if (ch < 32 || ch > 90) ch = '?';
  uint8_t idx = (uint8_t)(ch - 32);
  for (uint8_t col = 0; col < 5; col++) {
    uint8_t colData = pgm_read_byte(&font5x7[idx][col]);
    for (uint8_t row = 0; row < 7; row++) {
      if (colData & (1 << row)) {
        fillRect(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}

// Draw a string at (x,y); returns pixel width used
int drawText(int x, int y, const String& txt, uint16_t color, uint8_t scale) {
  int cx = x;
  for (unsigned int i = 0; i < txt.length(); i++) {
    char ch = toupper((unsigned char)txt[i]);
    drawChar(cx, y, ch, color, scale);
    cx += (5 + 1) * scale;
  }
  return cx - x;
}

// Display a speech bubble with message for 4 seconds, then restore animation
void showMessage(const String& txt) {
  const uint8_t scale = 2;
  const int charW  = (5 + 1) * scale;
  const int charH  = 7 * scale;
  const int pad    = 10;
  int len          = min((int)txt.length(), 24);
  int bubbleW      = len * charW + pad * 2;
  int bubbleH      = charH + pad * 2;
  int bx           = (320 - bubbleW) / 2;
  int by           = 240 - bubbleH - 12;

  // White rounded bubble (3 overlapping rects approximate rounded corners)
  fillRect(bx,      by + 4,  bubbleW,     bubbleH - 8, COLOR_WHITE);
  fillRect(bx + 4,  by,      bubbleW - 8, bubbleH,     COLOR_WHITE);
  fillRect(bx + 2,  by + 2,  bubbleW - 4, bubbleH - 4, COLOR_WHITE);

  // Tail pointing up toward face
  fillRect(156, by - 6, 8, 8, COLOR_WHITE);
  fillRect(158, by - 10, 4, 6, COLOR_WHITE);

  // Text in black
  drawText(bx + pad, by + pad, txt.substring(0, len), COLOR_BLACK, scale);

  // Hold 4 seconds then erase and restore
  delay(4000);
  fillScreen(COLOR_BLACK);
  if (weatherModeActive) {
    extern void drawWeatherDashboard();
    drawWeatherDashboard();
  } else {
    renderCurrentFrame();
  }
}

// Draw the dedicated weather/time dashboard
void drawWeatherDashboard() {
  fillScreen(COLOR_BLACK);
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d:%02d", getRTCHour(), getRTCMinute());
  drawText(70, 40, buf, COLOR_CYAN, 5); 
  
  snprintf(buf, sizeof(buf), "TEMP: %.1f C", lastTemp);
  drawText(20, 120, buf, COLOR_RED, 3);
  
  snprintf(buf, sizeof(buf), "HUM : %.1f %%", lastHumi);
  drawText(20, 180, buf, COLOR_PINK, 3);
}

// ==========================================
// EMOJI COMPONENTS (same as original)
// ==========================================


void drawOpenEye(int cx, int cy) {
  fillCircle(cx,cy,26,COLOR_EYE_GREY); fillCircle(cx,cy,10,COLOR_BLACK);
  fillCircle(cx+4,cy-4,3,COLOR_WHITE);
}
void drawSquintEye(int cx,int cy){
  fillCircle(cx,cy,26,COLOR_EYE_GREY);
  for(int y=-30;y<8;y++) for(int x=-30;x<=30;x++) if(y<(x*3/10)-2) fillRect(cx+x,cy+y,1,1,COLOR_BLACK);
  fillCircle(cx,cy+8,10,COLOR_BLACK); fillCircle(cx+4,cy+6,3,COLOR_WHITE);
  for(int x=-25;x<=25;x++) fillRect(cx+x,cy+(x*3/10)-2,2,4,COLOR_EYE_GREY);
}
void drawClosedEye(int cx,int cy){
  for(int a=15;a<=165;a++){
    float rad=a*3.14159/180.0;
    int x=cx-cos(rad)*26, y=cy+sin(rad)*12;
    fillRect(x,y,3,3,COLOR_EYE_GREY);
    if(a==40||a==75||a==105||a==140) fillRect(x,y,2,10,COLOR_EYE_GREY);
  }
}
void drawKissMouth(int cx,int cy){
  for(int a=-90;a<=90;a++){
    float rad=a*3.14159/180.0;
    fillRect(cx+cos(rad)*10,cy-8+sin(rad)*8,4,4,COLOR_DARKRED);
    fillRect(cx+cos(rad)*10,cy+8+sin(rad)*8,4,4,COLOR_DARKRED);
  }
}
void drawHeart(int cx,int cy,int size){
  int r=size/2;
  fillCircle(cx-r,cy,r,COLOR_RED); fillCircle(cx+r,cy,r,COLOR_RED);
  for(int i=0;i<=size;i++) fillRect(cx-(size-i),cy+i+(r/2)-1,(size-i)*2+1,2,COLOR_RED);
  fillCircle(cx-r+1,cy-1,1,COLOR_WHITE);
}
void drawEyePupilOffset(int cx,int cy,int ox,int oy){
  fillCircle(cx,cy,26,COLOR_EYE_GREY); fillCircle(cx+ox,cy+oy,10,COLOR_BLACK);
  fillCircle(cx+ox+4,cy+oy-4,3,COLOR_WHITE);
}
void drawSmileMouth(int cx,int cy){
  fillCircle(cx,cy,12,COLOR_DARKRED); fillRect(cx-15,cy-15,30,15,COLOR_BLACK);
}
void drawArrowWink(int cx,int cy){
  for(int x=0;x<28;x++){
    int yo=x*12/28;
    fillRect(cx-14+x,cy-12+yo,4,4,COLOR_EYE_GREY);
    fillRect(cx-14+x,cy+12-yo,4,4,COLOR_EYE_GREY);
  }
}
void drawSadEye(int cx,int cy,bool isLeft){
  fillCircle(cx,cy,26,COLOR_EYE_GREY);
  int px=cx+(isLeft?8:-8); fillCircle(px,cy+5,9,COLOR_BLACK); fillCircle(px-2,cy+2,3,COLOR_CYAN);
  for(int y=-30;y<5;y++) for(int x=-30;x<=30;x++){
    int my=isLeft?(-x/3)-10:(x/3)-10;
    if(y<my) fillRect(cx+x,cy+y,1,1,COLOR_BLACK);
  }
}
void drawFrownMouth(int cx,int cy){
  fillCircle(cx,cy,12,COLOR_DARKRED); fillRect(cx-15,cy,30,15,COLOR_BLACK);
}
void drawTear(int cx,int cy){
  fillCircle(cx,cy,4,COLOR_TEAR);
  for(int i=0;i<6;i++){ int w=max(1,4-i*4/6); fillRect(cx-w,cy-i-3,w*2+1,1,COLOR_TEAR); }
  fillCircle(cx+1,cy-1,1,COLOR_WHITE);
}
void eraseTear(int cx,int cy){ fillRect(cx-5,cy-10,11,16,COLOR_BLACK); }
void drawSleepEye(int cx,int cy){
  fillCircle(cx,cy,26,COLOR_EYE_GREY); fillRect(cx-30,cy-30,60,30,COLOR_BLACK);
  fillCircle(cx,cy-8,28,COLOR_BLACK);
}
void drawSnoreMouth(int cx,int cy){
  fillCircle(cx,cy,10,COLOR_DARKRED); fillCircle(cx,cy,4,COLOR_BLACK);
}
void drawZ(int cx,int cy,int s){
  int t=s>12?3:2;
  fillRect(cx,cy,s,t,COLOR_TEAR); fillRect(cx,cy+s-t,s,t,COLOR_TEAR);
  for(int i=0;i<s;i++) fillRect(cx+s-1-i-t/2,cy+i,t,2,COLOR_TEAR);
}
void eraseZ(int cx,int cy,int s){ fillRect(cx-2,cy-2,s+4,s+4,COLOR_BLACK); }
void drawChevronEye(int cx,int cy,bool isLeft){
  for(int x=0;x<30;x++){
    int yo=x*12/30, dx=isLeft?(cx-15+x):(cx+15-x);
    fillRect(dx,cy-12+yo,4,4,COLOR_EYE_GREY);
    fillRect(dx,cy+12-yo,4,4,COLOR_EYE_GREY);
  }
}
void drawCuteEye(int cx,int cy,bool isLeft){
  fillCircle(cx,cy,26,COLOR_EYE_GREY);
  int px=cx+(isLeft?5:-5); fillCircle(px,cy,10,COLOR_BLACK); fillCircle(px+4,cy-4,3,COLOR_WHITE);
  fillRect(cx-1,cy-36,2,10,COLOR_EYE_GREY);
  for(int i=0;i<8;i++){ fillRect(cx-18-i,cy-22-i,2,2,COLOR_EYE_GREY); fillRect(cx+18+i,cy-22-i,2,2,COLOR_EYE_GREY); }
}
void drawCuteClosedEye(int cx,int cy){
  for(int a=15;a<=165;a++){
    float rad=a*3.14159/180.0;
    int x=cx-cos(rad)*26, y=cy+sin(rad)*12;
    fillRect(x,y,3,3,COLOR_EYE_GREY);
    if(a==45||a==90||a==135) fillRect(x,y+2,2,8,COLOR_EYE_GREY);
  }
}
void drawCuteMouth(int cx,int cy){
  fillCircle(cx,cy,35,COLOR_DARKRED); fillRect(cx-40,cy-35,80,35,COLOR_BLACK);
}
void drawPleadingEye(int cx,int cy,bool isLeft,int ox,int oy){
  fillCircle(cx,cy,26,COLOR_EYE_GREY);
  for(int y=-30;y<10;y++) for(int x=-30;x<=30;x++){
    int my=isLeft?(-x*8/10)-5:(x*8/10)-5;
    if(y<my) fillRect(cx+x,cy+y,1,1,COLOR_BLACK);
  }
  fillCircle(cx+ox,cy+oy,10,COLOR_BLACK); fillCircle(cx+ox+4,cy+oy-4,3,COLOR_WHITE);
}
void drawPuppySnout(int cx,int cy){
  fillOval(cx,cy,48,18,COLOR_EYE_GREY); fillCircle(cx,cy+12,10,COLOR_BROWN);
}
void drawStrawCup(){
  fillRect(165,146,25,8,COLOR_EYE_GREY); fillRect(182,146,8,45,COLOR_EYE_GREY);
  fillRect(158,185,56,6,COLOR_DARKGRAY); fillRect(163,191,46,35,COLOR_EYE_GREY);
}
void drawDrinkDrop(int f){
  fillRect(184,148,4,35,COLOR_EYE_GREY); fillRect(165,148,19,4,COLOR_EYE_GREY);
  if(f==1) fillRect(184,174,4,10,COLOR_TEAR);
  else if(f==2) fillRect(184,156,4,10,COLOR_TEAR);
  else if(f==3) fillRect(170,148,10,4,COLOR_TEAR);
}

// ==========================================
// ANIMATION FRAMES (same as original)
// ==========================================

void drawKissFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawOpenEye(110,90); drawOpenEye(210,90); drawKissMouth(160,150); }
  else if(f==1){ fillRect(60,40,200,80,COLOR_BLACK); drawSquintEye(110,90); drawClosedEye(210,90); }
  else if(f==2){ fillRect(60,40,100,80,COLOR_BLACK); drawClosedEye(110,90); drawHeart(185,155,12); }
  else if(f==3) drawHeart(220,185,9);
  else if(f==4) drawHeart(190,215,10);
}
void drawLookFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawEyePupilOffset(110,90,0,0); drawEyePupilOffset(210,90,0,0); drawSmileMouth(160,150); }
  else if(f==1){ fillRect(60,40,200,80,COLOR_BLACK); drawEyePupilOffset(110,90,-10,0); drawEyePupilOffset(210,90,-10,0); }
  else if(f==2){ fillRect(60,40,200,80,COLOR_BLACK); drawEyePupilOffset(110,90,10,0); drawEyePupilOffset(210,90,10,0); }
  else if(f==3){ fillRect(60,40,200,80,COLOR_BLACK); drawArrowWink(110,90); drawEyePupilOffset(210,90,0,0); }
  else if(f==4){ fillRect(60,40,200,80,COLOR_BLACK); drawEyePupilOffset(110,90,0,0); drawEyePupilOffset(210,90,0,0); }
}
void drawCryFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawSadEye(110,90,true); drawSadEye(210,90,false); drawFrownMouth(160,150); }
  else if(f==1) drawTear(220,125);
  else if(f==2){ eraseTear(220,125); drawTear(220,155); drawTear(100,125); }
  else if(f==3){ eraseTear(220,155); eraseTear(100,125); drawTear(220,195); drawTear(100,155); }
}
void drawSleepFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawSleepEye(110,90); drawSleepEye(210,90); drawSnoreMouth(160,150); drawZ(180,150,8); }
  else if(f==1){ eraseZ(180,150,8); eraseZ(210,135,12); eraseZ(240,120,16); eraseZ(270,105,20); drawZ(210,135,12); drawZ(180,150,8); }
  else if(f==2){ eraseZ(180,150,8); eraseZ(210,135,12); eraseZ(240,120,16); eraseZ(270,105,20); drawZ(240,120,16); drawZ(210,135,12); drawZ(180,150,8); }
  else if(f==3){ eraseZ(180,150,8); eraseZ(210,135,12); eraseZ(240,120,16); eraseZ(270,105,20); drawZ(270,105,20); drawZ(240,120,16); drawZ(210,135,12); }
}
void drawLaughCryFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawChevronEye(110,90,true); drawChevronEye(210,90,false); fillCircle(160,150,45,COLOR_DARKRED); fillRect(110,105,100,45,COLOR_BLACK); }
  else if(f==1){ drawTear(85,100); drawTear(235,100); }
  else if(f==2){ eraseTear(85,100); eraseTear(235,100); drawTear(85,130); drawTear(235,130); }
  else if(f==3){ eraseTear(85,130); eraseTear(235,130); drawTear(85,160); drawTear(235,160); }
  else if(f==4){ eraseTear(85,160); eraseTear(235,160); drawTear(85,190); drawTear(235,190); }
}
void drawCuteBlinkFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawCuteEye(110,90,true); drawCuteEye(210,90,false); drawCuteMouth(160,160); }
  else if(f==1){ fillRect(60,40,200,95,COLOR_BLACK); drawCuteClosedEye(110,90); drawCuteClosedEye(210,90); }
  else if(f==2){ fillRect(60,40,200,95,COLOR_BLACK); drawCuteEye(110,90,true); drawCuteEye(210,90,false); }
}
void drawPleadingFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawPleadingEye(110,90,true,0,5); drawPleadingEye(210,90,false,0,5); drawPuppySnout(160,160); }
  else if(f==1){ fillRect(60,40,200,80,COLOR_BLACK); drawPleadingEye(110,90,true,8,5); drawPleadingEye(210,90,false,8,5); drawTear(230,120); }
  else if(f==2){ fillRect(60,40,200,80,COLOR_BLACK); drawPleadingEye(110,90,true,-8,5); drawPleadingEye(210,90,false,-8,5); eraseTear(230,120); drawTear(230,150); drawTear(90,120); }
  else if(f==3){ fillRect(60,40,200,80,COLOR_BLACK); drawPleadingEye(110,90,true,0,8); drawPleadingEye(210,90,false,0,8); eraseTear(230,150); eraseTear(90,120); drawTear(230,180); drawTear(90,150); }
  else if(f==4){ fillRect(60,40,200,80,COLOR_BLACK); drawPleadingEye(110,90,true,0,2); drawPleadingEye(210,90,false,0,2); eraseTear(230,180); eraseTear(90,150); drawTear(90,180); }
}
void drawDrinkFrame(int f){
  if(f==0){ fillScreen(COLOR_BLACK); drawChevronEye(110,90,true); drawChevronEye(210,90,false); drawStrawCup(); drawKissMouth(160,150); drawDrinkDrop(0); }
  else drawDrinkDrop(f);
}
void renderCurrentFrame(){
  if(currentAnimation==0) drawKissFrame(currentFrame);
  else if(currentAnimation==1) drawLookFrame(currentFrame);
  else if(currentAnimation==2) drawCryFrame(currentFrame);
  else if(currentAnimation==3) drawSleepFrame(currentFrame);
  else if(currentAnimation==4) drawLaughCryFrame(currentFrame);
  else if(currentAnimation==5) drawCuteBlinkFrame(currentFrame);
  else if(currentAnimation==6) drawPleadingFrame(currentFrame);
  else if(currentAnimation==7) drawDrinkFrame(currentFrame);
}

// ==========================================
// BUZZER (passive, LEDC PWM)
// ==========================================

void buzzTone(int freq, int durationMs) {
  if (freq == 0) { ledcWrite(BUZZER_PIN, 0); delay(durationMs); return; }
  ledcWriteTone(BUZZER_PIN, freq);
  ledcWrite(BUZZER_PIN, 128); // 50% duty
  delay(durationMs);
  ledcWrite(BUZZER_PIN, 0);
}

// pattern: 0=startup, 1=happy, 2=sad, 3=sleep, 4=hot-alert, 5=click
void playTone(int pattern) {
  switch (pattern) {
    case 0: // Startup jingle — ascending three notes
      buzzTone(523, 120); delay(40);
      buzzTone(659, 120); delay(40);
      buzzTone(784, 200);
      break;
    case 1: // Happy — quick double chirp
      buzzTone(880, 80); delay(30); buzzTone(1046, 120);
      break;
    case 2: // Sad — descending two notes
      buzzTone(440, 150); delay(30); buzzTone(330, 250);
      break;
    case 3: // Sleep — soft low hum blip
      buzzTone(220, 80); delay(200); buzzTone(196, 80);
      break;
    case 4: // Hot alert — three fast beeps
      for(int i=0;i<3;i++){ buzzTone(1200,60); delay(60); }
      break;
    default: // Click feedback
      buzzTone(600, 30);
      break;
  }
}

// Play tone that matches current animation
void playAnimTone(int anim) {
  if(anim==0||anim==5) playTone(1);      // Kiss, Blink → happy
  else if(anim==2||anim==6) playTone(2); // Cry, Plead → sad
  else if(anim==3) playTone(3);           // Sleep → sleep tone
  else playTone(5);                        // Others → click
}

// ==========================================
// DS3231 RTC via Wire (I2C addr 0x68)
// ==========================================

uint8_t rtcRead(uint8_t reg) {
  Wire.beginTransmission(0x68);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x68, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}
uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
int getRTCHour()   { return bcd2dec(rtcRead(0x02) & 0x3F); }
int getRTCMinute() { return bcd2dec(rtcRead(0x01)); }

// Return scheduled animation index based on time-of-day
int getScheduledAnimation(int hour) {
  if (hour >= 21 || hour < 7) return 3; // Nighttime (9 PM to 7 AM) → Sleeping
  return 1;                             // Normal time → Looking
}

// ==========================================
// DHT11 — TEMPERATURE / HUMIDITY
// ==========================================

void notifySensors() {
  if (!sensChar || !bleConnected) return;
  char buf[32];
  snprintf(buf, sizeof(buf), "T:%.0f H:%.0f", lastTemp, lastHumi);
  sensChar->setValue((uint8_t*)buf, strlen(buf));
  sensChar->notify();
}

void readSensors() {
  // --- DHT11 ---
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    lastTemp = t;
    lastHumi = h;
    Serial.printf("Temp: %.1f°C  Humi: %.1f%%\n", t, h);

    // Auto-mood: hot (>32°C) → LaughCry, cold (<16°C) → Sleep
    if (!autoMoodActive) userAnimation = currentAnimation;
    if (t > 32.0) {
      if (currentAnimation != 4) {
        autoMoodActive = true;
        currentAnimation = 4; currentFrame = 0;
        fillScreen(COLOR_BLACK); renderCurrentFrame();
        playTone(4); // hot alert beep
        showMessage("ITS HOT IN HERE");
      }
    } else if (t < 16.0) {
      if (currentAnimation != 3) {
        autoMoodActive = true;
        currentAnimation = 3; currentFrame = 0;
        fillScreen(COLOR_BLACK); renderCurrentFrame();
        showMessage("BRRR SO COLD");
      }
    } else {
      // Temp normal — restore user animation
      if (autoMoodActive) {
        autoMoodActive = false;
        currentAnimation = userAnimation; currentFrame = 0;
        fillScreen(COLOR_BLACK); renderCurrentFrame();
      }
    }
  }

  // --- Notify Flutter ---
  notifySensors();
}

// ==========================================
// TFT INIT
// ==========================================


void tft_init(){
  digitalWrite(TFT_RST,LOW); delay(20); digitalWrite(TFT_RST,HIGH); delay(120);
  writeCmd(0xEF); writeData(0x03); writeData(0x80); writeData(0x02);
  writeCmd(0xCF); writeData(0x00); writeData(0xC1); writeData(0x30);
  writeCmd(0xED); writeData(0x64); writeData(0x03); writeData(0x12); writeData(0x81);
  writeCmd(0xE8); writeData(0x85); writeData(0x00); writeData(0x78);
  writeCmd(0xCB); writeData(0x39); writeData(0x2C); writeData(0x00); writeData(0x34); writeData(0x02);
  writeCmd(0xF7); writeData(0x20);
  writeCmd(0xEA); writeData(0x00); writeData(0x00);
  writeCmd(0xC0); writeData(0x23);
  writeCmd(0xC1); writeData(0x10);
  writeCmd(0xC5); writeData(0x3e); writeData(0x28);
  writeCmd(0xC7); writeData(0x86);
  writeCmd(0x36); writeData(0x28);
  writeCmd(0x3A); writeData(0x55);
  writeCmd(0xB1); writeData(0x00); writeData(0x18);
  writeCmd(0xB6); writeData(0x08); writeData(0x82); writeData(0x27);
  writeCmd(0xF2); writeData(0x00);
  writeCmd(0x26); writeData(0x01);
  writeCmd(0x11); delay(120);
  writeCmd(0x29); delay(120);
}

// ==========================================
// SETUP
// ==========================================

void setup(){
  Serial.begin(115200);

  // TFT
  pinMode(TFT_DC,OUTPUT); pinMode(TFT_CS,OUTPUT);
  pinMode(TFT_RST,OUTPUT);
  digitalWrite(TFT_CS,HIGH);
  SPI.begin(TFT_CLK,TFT_MISO,TFT_MOSI,TFT_CS);
  SPI.beginTransaction(SPISettings(40000000,MSBFIRST,SPI_MODE0));
  tft_init();

  // Backlight via LEDC (ESP32 Core 3.x API)
  ledcAttach(TFT_LED, 5000, 8);          // 5kHz, 8-bit
  ledcWrite(TFT_LED, 255);               // Full brightness at boot

  // Buzzer via LEDC (ESP32 Core 3.x API)
  ledcAttach(BUZZER_PIN, 2000, 8);       // 2kHz default, 8-bit

  // DHT11
  dht.begin();

  // Wire / I2C for DS3231 on soft pins 26+27
  Wire.begin(I2C_SDA, I2C_SCL);

  // BLE Setup
  BLEDevice::init("EMO Bot");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* svc = server->createService(BLEUUID(SERVICE_UUID), 20); // extra handles for 4 chars

  // AnimChar — write animation index
  BLECharacteristic* animChar = svc->createCharacteristic(
    ANIM_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  animChar->setCallbacks(new AnimCharCallbacks());

  // TextChar — write message string
  BLECharacteristic* textChar = svc->createCharacteristic(
    TEXT_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  textChar->setCallbacks(new TextCharCallbacks());

  // SensChar — notify sensor data to Flutter
  sensChar = svc->createCharacteristic(
    SENS_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  sensChar->addDescriptor(new BLE2902());

  // BuzzChar — app triggers a tone pattern
  BLECharacteristic* buzzChar = svc->createCharacteristic(
    BUZZ_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  buzzChar->setCallbacks(new BuzzCharCallbacks());

  svc->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("EMO Bot ready. Advertising...");

  // Startup jingle + first frame
  playTone(0);
  renderCurrentFrame();
}

// ==========================================
// LOOP
// ==========================================

void loop(){
  unsigned long now = millis();

  // --- BLE reconnect ---
  if(!bleConnected && bleWasConnected){
    delay(500); BLEDevice::startAdvertising(); bleWasConnected=false;
  }
  bleWasConnected = bleConnected;

  // --- Capacitive touch: short press = love, long press = temp, double tap = weather mode ---
  static unsigned long touchStartMillis = 0;
  static bool isTouching = false;
  static unsigned long lastReleaseMillis = 0;
  int tv = touchRead(TOUCH_PIN);
  
  if(tv > 0 && tv < 40) { // Currently touching
    if(!isTouching) {
      isTouching = true;
      touchStartMillis = now;
    } else if(now - touchStartMillis > 3000) { // Held for 3 seconds
      isTouching = false; // Reset to avoid triggering repeatedly
      touchStartMillis = now + 5000; // Debounce
      playTone(5); // Click acknowledge
      char buf[32];
      snprintf(buf, sizeof(buf), "TEMP IS %.1f C", lastTemp);
      showMessage(buf);
    }
  } else {
    if(isTouching) {
      // Released before 3 seconds = short press
      if(now - touchStartMillis < 3000 && now - lastTouchTime > 400) {
        if (now - lastReleaseMillis < 600) {
          // Double Tap -> Toggle Weather Mode
          weatherModeActive = !weatherModeActive;
          touchOverrideEnd = 0; // Cancel love override
          if (weatherModeActive) {
            drawWeatherDashboard();
            playTone(1);
          } else {
            fillScreen(COLOR_BLACK); renderCurrentFrame();
          }
        } else {
          // Single Tap -> Love Animation
          if (!weatherModeActive) {
            currentAnimation = 0; // Kissing/Loving animation
            currentFrame = 0; lastTouchTime = now; lastFrameTime = now;
            fillScreen(COLOR_BLACK); renderCurrentFrame();
            playTone(1); // Happy chirp
            touchOverrideEnd = now + 5000; // Hold love for 5 seconds
          }
        }
        lastReleaseMillis = now;
      }
      isTouching = false;
    }
  }

  // --- Touch override timeout ---
  if(touchOverrideEnd > 0 && now > touchOverrideEnd) {
    touchOverrideEnd = 0;
    if(!autoMoodActive) {
      currentAnimation = userAnimation; // Revert to normal
      currentFrame = 0; lastFrameTime = now;
      fillScreen(COLOR_BLACK); renderCurrentFrame();
    }
  }

  // --- Sensor poll every 30 seconds ---
  if(now - lastSensorRead > 30000UL){
    lastSensorRead = now;
    readSensors();
  }

  // --- RTC time-based mood, Hourly Chime, & Smart Alarm ---
  static unsigned long lastRTCCheck = 0;
  static int lastRTCHour = -1;
  if(now - lastRTCCheck > 60000UL){
    lastRTCCheck = now;
    int h = getRTCHour();
    int m = getRTCMinute();
    
    // Software Feature: Hourly Chime
    if (h != lastRTCHour && m == 0 && lastRTCHour != -1) {
      playTone(0);
    }

    // Software Feature: Smart Alarm Clock
    if (alarmActive && h == alarmHour && m == alarmMin) {
      alarmActive = false;
      weatherModeActive = false;
      playTone(4); playTone(4); // LOUD alarm
      showMessage("WAKE UP!");
    }

    if(h != lastRTCHour && !autoMoodActive){
      lastRTCHour = h;
      int scheduled = getScheduledAnimation(h);
      if(scheduled != currentAnimation){
        userAnimation = scheduled;
        currentAnimation = scheduled;
        if (!weatherModeActive) {
          currentFrame = 0; lastFrameTime = now;
          fillScreen(COLOR_BLACK); renderCurrentFrame();
          playAnimTone(currentAnimation);
        }
        Serial.printf("RTC hour=%d → animation %d\n", h, scheduled);
      }
    }
  }

  // --- Pomodoro / Focus Timer ---
  if (pomoActive) {
    if (now > pomoEndTime) {
      pomoActive = false;
      userAnimation = 7; // Drinking / Break
      currentAnimation = 7;
      weatherModeActive = false;
      fillScreen(COLOR_BLACK); renderCurrentFrame();
      playTone(4); // Beep beep beep
      showMessage("BREAK TIME!");
    }
  }

  // --- Animation frame timing ---
  int delayMs=0;
  if(currentAnimation==0){      int t[]={1500,250,300,300,1500}; delayMs=t[currentFrame]; }
  else if(currentAnimation==1){ int t[]={1500,800,800,800,1500};  delayMs=t[currentFrame]; }
  else if(currentAnimation==2){ int t[]={1500,400,400,1000};      delayMs=t[currentFrame]; }
  else if(currentAnimation==3){ delayMs=800; }
  else if(currentAnimation==4){ int t[]={1200,250,250,250,600};   delayMs=t[currentFrame]; }
  else if(currentAnimation==5){ int t[]={2000,150,1500,150,150};  delayMs=t[currentFrame]; }
  else if(currentAnimation==6){ int t[]={1000,500,500,500,800};   delayMs=t[currentFrame]; }
  else if(currentAnimation==7){ int t[]={600,150,150,150};        delayMs=t[currentFrame]; }

  if(now-lastFrameTime>delayMs && !weatherModeActive){
    lastFrameTime=now;
    currentFrame++;
    if(currentFrame>=maxFrames[currentAnimation]) currentFrame=0;
    renderCurrentFrame();
  }
}
