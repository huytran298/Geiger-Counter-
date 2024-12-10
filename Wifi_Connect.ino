#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_I2CDevice.h>
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>  
// Khởi tạo server trên cổng 80
AsyncWebServer server(80);

// Tên và mật khẩu WiFi
const char *ssid = "ToiLaTrungTin";
const char *password = "tindangiu";
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
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
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
// Hàm để xử lý kết nối WiFi
void connectToWiFi(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

String generateWiFiForm() {
    int n = WiFi.scanNetworks();
    String form = "<form action=\"/connect\" method=\"POST\">";
    
    if (n == 0) {
        form += "No networks found.";
    } else {
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            form += "<div style='display: flex; margin-bottom: 10px;'>";
            form += "<label style='width: 150px;'>" + ssid + "</label>";
            form += "<input type='hidden' name='ssid" + String(i) + "' value='" + ssid + "'>";
            form += "<input type='password' name='password" + String(i) + "' placeholder='Password'>";
            form += "</div>";
        }
    }

    form += "<input type='submit' value='Connect'>";
    form += "</form>";
    
    WiFi.scanDelete(); // Xóa danh sách scan sau khi sử dụng
    return form;
}
void setup() {
    Serial.begin(115200);
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
    
    tft.fillScreen(ST77XX_BLACK);

    lastStateA = digitalRead(pinA);
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.println(WiFi.softAPIP());
    
    server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request){
        int params = request->params();
        for (int i = 0; i < params; i++) {
            const AsyncWebParameter* p = request->getParam(i);
            if (p->name().startsWith("ssid")) {
                String ssid = p->value();
                String password = request->getParam("password" + p->name().substring(4), true)->value();
                if(password == "" || password == NULL) continue;
                Serial.printf("Connecting to SSID: %s with Password: %s\n", ssid.c_str(), password.c_str());
                
                //connectToWiFi(ssid.c_str(), password.c_str());
                WiFi.begin(ssid.c_str(), password.c_str());
                request->send(200, "text/plain", "Processing connections...");
                int cnt = 0;
                while (WiFi.status() != WL_CONNECTED && cnt < 5000) {
                    delay(500);
                    cnt += 500;
                    Serial.print(".");
                }
                if(cnt >= 5000){
                  request->redirect("/?error=1");
                }else {
                  Serial.println("\nConnected to WiFi!");
                  request->send(200, "text/plain", "Connected to WiFi!");
                }
                break;
            }
        }
        
        
    });
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          String form = "<h1>WiFi Logging</h1>";
          form += generateWiFiForm();  // Hàm này tạo form hiển thị danh sách SSID
          
          // JavaScript kiểm tra lỗi từ query string
          form += "<script>"
                  "const urlParams = new URLSearchParams(window.location.search);"
                  "if (urlParams.has('error')) {"
                  "  alert('Failed to connect to WiFi. Please try again.');"
                  "}"
                  "</script>";
          
          request->send(200, "text/html", form);
      });
    server.begin();
   
  
}

void loop() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
    // Kiểm tra trạng thái WiFi
    if (WiFi.status() == WL_CONNECTED) {
        //Serial.println("Connected successfully!");
        tft.setCursor(10, 50);
        tft.setTextColor(ST77XX_BLUE);
        tft.print("Wifi is Connected");
        Serial.println("Connected to WiFi");
        int n, color;
        int rssi = abs(WiFi.RSSI());
        if(rssi <= 60)n = 4, color = ST77XX_GREEN;
        else if(rssi <= 70)n = 3, color = ST77XX_YELLOW;
        else if(rssi <= 80)n = 2, color = ST77XX_ORANGE;
        else n = 1, color = ST77XX_RED;
        //tft.fillScreen(ST77XX_BLACK);
        for(int i = 0; i < n; i ++){
          tft.fillRect(290 + 8 * i, 20 - 5 * i, 5, 5 * (i + 1), color);
        }
        delay(500); // Giảm tần suất log
      
    } else {
        Serial.println("Not connected to WiFi");
        tft.setCursor(10, 50);
        tft.setTextColor(ST77XX_BLUE);
        tft.print("Connect Wifi...");
        delay(500);
    }
}
