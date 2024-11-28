#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_I2CDevice.h>
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>             // Arduino SPI library
#include <image.h>

//thư viện

#define TFT_MOSI 11 // In some display driver board, it might be written as "SDA" and so on.
#define TFT_SCLK 12
#define TFT_CS   10  // Chip select control pin
#define TFT_DC   8  // Data Command control pin
#define TFT_RST  13  // Reset pin (could connect to Arduino RESET pin)
#define DEBOUNCE_TIME 190 // According to ebay specifications, read in microsecs

const int pinA = 3;        // Chân A của EC11
const int pinB = 4;        // Chân B của EC11
const int pinButton = 5;   // Chân SW của EC11
const int PWMPin = 2;
const int IN_PORT = 1;

//chân kết nối

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Cài đặt thư viện

volatile int counter = 0;  // Biến đếm
volatile int precounter = -1;
volatile bool counting = false; // Trạng thái đếm
int lastStateA;            // Trạng thái trước đó của chân A
unsigned long currentMillis;
uint64_t tube_elapsed = 0; // Used for checking tube debounce time
unsigned long previousMillis, prevMinutes, prevPress, scr_upd;
uint16_t cps, cpm = 0;
float cf;
uint16_t counts = 0, countsMinute;
//hàm 
int16_t nums[5] = { 0, 0, 1, 7, 5 }, bits[5], state = 5;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < 5; i++) {
    bits[i] = 1;
  }
  bits[0] = 0;
  // Input 
  pinMode(pinA, INPUT);
  pinMode(pinB, INPUT);
  pinMode(pinButton, INPUT);
  pinMode(IN_PORT, INPUT);
  // Output 
  pinMode(PWMPin, OUTPUT);
  pinMode(7, OUTPUT);

  ledcAttach(PWMPin, 3500, 8);
  ledcWrite(PWMPin, 70);
  analogWrite(7, 64);

  tft.init(170, 320, SPI_MODE2);    // Init ST7789 display 135x240 pixel
  tft.setRotation(3);
  delay(10);
  tft.drawRGBBitmap(0, 0, Image, 320, 170);
  delay(1000);
  tft.fillScreen(ST77XX_BLACK);

  lastStateA = digitalRead(pinA);

  attachInterrupt(digitalPinToInterrupt(IN_PORT), tube_impulse, FALLING);
  // Interrupt
}
void tube_impulse() {
  if (micros() - tube_elapsed > DEBOUNCE_TIME) {
    counts++;
    tube_elapsed = micros();
  }
}
void Countings() {
  // unused variable
  // uint16_t lastButtonPress = millis();

  if (digitalRead(pinButton) == LOW &&
    currentMillis - prevPress >= 500) {

    if (state >= 5 && !counting) {
      counting = true;
      state = 0;
      tft.setCursor(220, 0);
      tft.setTextColor(ST77XX_RED);
      tft.print("FIX");
      for (int i = 0; i < 5; i++) {
        if (i != state)bits[i] = 1;
        else bits[i] = 0;
      }
      //return;
    }
    else {
      state++;
      if (state >= 5) {
        // state = 0;
        tft.setCursor(250, 0);
        tft.setTextColor(ST77XX_RED);
        tft.print("OK");
        counting = false;
        //return;
      }
      else {
        for (int i = 0; i < 5; i++) {
          if (i != state)bits[i] = 1;
          else bits[i] = 0;
        }
      }
    }
    prevPress = currentMillis;
    //counter = 0;
  }
  readEncoder();
}

void loop() {
  // what is this ? 
  // unused variable 
  // uint8_t tem = millis();

  currentMillis = millis();
  Countings();
  if (!counting) {
    cf = 0;
    for (int i = 0; i < 5; i++) cf = cf * 10 + nums[i];
    cf *= 0.00001;
  }
  if (currentMillis - scr_upd >= 200) {
    scr_upd = currentMillis;
    screen_update();
  }
  if (currentMillis - previousMillis >= 1000) {
    //Serial.println(currentMillis);
    previousMillis = currentMillis;
    cps = counts;
    if (currentMillis - prevMinutes >= 15000) {
      cpm = countsMinute + cps;
      prevMinutes = currentMillis;
      countsMinute = 0;
    }
    else countsMinute += cps;
    counts = 0;

    Serial.print("cps: ");
    Serial.println(cps);

  }
}
void screen_update() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);

  //print cps
  tft.setCursor(10, 10);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("CPS: ");
  //tft.setCursor(110, 10);

  tft.setTextColor(ST77XX_RED);
  tft.println(cps);

  //print cpm
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("CPM: ");
  //tft.setCursor(110, 50);
  tft.setTextColor(ST77XX_RED);
  tft.println(cpm * 4);

  // print uSv 
  tft.setCursor(10, 90);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("uSv/h: ");
  tft.setCursor(150, 90);
  tft.setTextColor(ST77XX_RED);
  tft.println((double)((double)(cpm * 4) * cf), 3);

  //print cf
  tft.setCursor(10, 130);
  tft.setTextColor(ST77XX_BLUE);
  tft.print("CF:");
  if (counting) {
    tft.print("0.");
    for (int i = 0; i < 5; i++) {
      if (bits[i] == 0)tft.setTextColor(ST77XX_WHITE);
      else tft.setTextColor(ST77XX_ORANGE);
      tft.print(nums[i]);
      Serial.print(nums[i]);
    }
    Serial.println();
  }
  else {
    tft.setTextColor(ST77XX_BLUE);
    tft.print(cf, 5);
  }

};
void readEncoder() {
  if (!counting) return; // Chỉ đếm khi đang bật chế độ đếm

  int stateA = digitalRead(pinA);
  int stateB = digitalRead(pinB);

  if (stateA != lastStateA) {
    if (stateA == HIGH) {
      prevPress = currentMillis;
      if (stateB == LOW) {
        nums[state]--; // Tăng nếu quay theo chiều kim đồng hồ
      }
      else {
        nums[state]++; // Giảm nếu quay ngược chiều kim đồng hồ
      }
      // declare as unint_16 but check for less than 0 ?? 
      if ((int16_t)nums[state] < 0)nums[state] = 9;
      else nums[state] %= 10;
    }
    lastStateA = stateA; // Cập nhật trạng thái trước đó
  }
}
