#include "esp_camera.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <WebServer.h>

WebServer server(80);

// Wi-Fi configuration
// const char* ssid = "311HHN Lau 1";
// const char* password = "@@1234abcdlau1";
// const char* ssid = "AndroidAP9B0A";
// const char* password = "quynhquynh";
const char* ssid = "Thai Bao";
const char* password = "0869334749";

const char* serverUrl = "http://192.168.1.119:8000/predict/";
const char* esp32Url = "http://192.168.1.120/receive-disease";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

const int API_CALL_HOUR = 6; 
const int API_CALL_MINUTE = 0;
const int API_CALL_SECOND = 0;

// GPIO pin definitions
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

TFT_eSPI tft = TFT_eSPI();
bool displayResults = false;
String resultText = "";
bool apiCallToday = false; // Variable to track if API was called today
int lastCallDay = -1;      // Last day API was called

#define COLOR_BACKGROUND  TFT_BLACK
#define COLOR_HEADER      TFT_NAVY
#define COLOR_TEXT        TFT_WHITE
#define COLOR_TIME        TFT_CYAN
#define COLOR_STATUS      TFT_GREEN
#define COLOR_WARNING     TFT_RED
#define COLOR_BORDER      TFT_DARKGREY

bool enableAPITest = true;
unsigned long lastAPITest = 0;

// Global variable to track the start time of result display
unsigned long resultDisplayStartTime = 0;
const unsigned long RESULT_DISPLAY_DURATION = 30000; // 30 seconds

// Global variables to store the fixed time of API result
String resultDate = "";
String resultTime = "";

void drawImageBorder(int x, int y, int w, int h) {
  tft.drawRect(x-2, y-2, w+4, h+4, COLOR_BORDER);
}

void cameraInit() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 1);
    s->set_saturation(s, 1);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 30);
    s->set_aec_value(s, 500);
    s->set_ae_level(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_denoise(s, 1);
    s->set_sharpness(s, 1);
    s->set_dcw(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_wpc(s, 1);
    s->set_bpc(s, 1);
  }
}

void displayWiFiConnectScreen() {
  tft.setRotation(3);
  
  for (int y = 0; y < tft.height(); y += 4) {
    uint16_t gradientColor = tft.color565(0, 0, map(y, 0, tft.height(), 0, 40));
    tft.fillRect(0, y, tft.width(), 4, gradientColor);
    delay(2);
  }
  
  tft.fillRoundRect(0, 0, tft.width(), 25, 5, COLOR_HEADER);
  tft.fillRect(0, 0, tft.width(), 20, COLOR_HEADER);
  tft.fillRect(0, 20, tft.width(), 5, tft.color565(0, 0, 80)); // Viền đậm ở dưới
  
  tft.setCursor(5, 5);
  tft.setTextColor(COLOR_TIME);
  tft.setTextSize(1);
  tft.print("WIRELESS");
  tft.setCursor(70, 5);
  tft.setTextColor(COLOR_TEXT);
  tft.print("NETWORK SETUP");
  
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2 - 30;
  
  for (int i = 0; i < 5; i++) {
    tft.drawRoundRect(centerX - 110 - i, centerY - 25 - i, 220 + i*2, 95 + i*2, 10, tft.color565(0, 40 + i*10, 80 + i*10));
    delay(30);
  }
  tft.fillRoundRect(centerX - 110, centerY - 25, 220, 95, 10, COLOR_HEADER);
  
  for (int i = 0; i < 90; i++) {
    uint16_t gradColor = tft.color565(0, 0, map(i, 0, 90, 50, 120));
    tft.drawRoundRect(centerX - 105 + i/2, centerY - 20 + i/2, 210 - i, 85 - i, 8, gradColor);
    if (i % 15 == 0) delay(5);
  }
  
  tft.setCursor(centerX - 80, centerY - 5);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.print("Connecting to Network:");
  
  tft.setCursor(centerX - 80, centerY + 15);
  tft.setTextColor(COLOR_TIME);
  tft.setTextSize(1);
  for (int i = 0; i < strlen(ssid); i++) {
    tft.print(ssid[i]);
    delay(20);
  }
  
  tft.drawRoundRect(centerX - 90, centerY + 35, 180, 12, 6, COLOR_BORDER);
  tft.drawRoundRect(centerX - 91, centerY + 34, 182, 14, 7, tft.color565(100, 100, 100));
  
  tft.drawFastHLine(centerX - 90, centerY + 35, 180, tft.color565(150, 150, 150));
  tft.drawFastVLine(centerX - 90, centerY + 35, 12, tft.color565(150, 150, 150));
  tft.drawFastHLine(centerX - 90, centerY + 46, 180, tft.color565(50, 50, 50));
  tft.drawFastVLine(centerX + 89, centerY + 35, 12, tft.color565(50, 50, 50));
  
  tft.fillRect(0, tft.height() - 30, tft.width(), 30, COLOR_BACKGROUND);
  tft.fillRect(0, tft.height() - 30, tft.width(), 2, tft.color565(60, 60, 60));
  
  tft.setCursor(centerX - 95, tft.height() - 20);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.print("Connecting to network");
  
  tft.print(".");
  delay(100);
  tft.print(".");
  delay(100);
  tft.print(".");
  delay(100);
}

void updateWiFiProgress(int progressPercentage, int attemptCount) {
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2 - 30;
  
  int progressWidth = map(progressPercentage, 0, 100, 0, 176);
  
  for (int i = 0; i < progressWidth; i++) {
    uint16_t progressColor = tft.color565(
      map(i, 0, 176, 0, 100),
      map(i, 0, 176, 150, 255),
      map(i, 0, 176, 100, 200)
    );
    tft.drawFastVLine(centerX - 88 + i, centerY + 37, 8, progressColor);
  }
  
  tft.drawFastHLine(centerX - 88, centerY + 37, progressWidth, tft.color565(200, 255, 255));
  
  tft.fillRect(centerX - 95, tft.height() - 20, 190, 15, COLOR_BACKGROUND);
  tft.setCursor(centerX - 95, tft.height() - 20);
  
  uint16_t textColor = tft.color565(
    200,
    map(progressPercentage, 0, 100, 200, 255),
    map(progressPercentage, 0, 100, 200, 255)
  );
  tft.setTextColor(textColor);
  
  String statusText = "Searching for network";
  tft.print(statusText);
  
  switch (attemptCount % 4) {
    case 0:
      tft.print(".");
      break;
    case 1:
      tft.print("..");
      break;
    case 2:
      tft.print("...");
      break;
    case 3:
      tft.print("....");
      break;
  }
  
  tft.fillRect(centerX + 60, tft.height() - 20, 40, 15, COLOR_BACKGROUND);
  tft.setCursor(centerX + 60, tft.height() - 20);
  tft.setTextColor(COLOR_TIME);
  tft.print(progressPercentage);
  tft.print("%");
}

void displayWiFiSuccess(String ipAddress) {
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2 - 30;
  
  for (int i = 0; i < 176; i++) {
    uint16_t progressColor = tft.color565(0, 255, map(i, 0, 176, 100, 150));
    tft.drawFastVLine(centerX - 88 + i, centerY + 37, 8, progressColor);
    if (i % 20 == 0) delay(5);
  }
  
  for (int i = 0; i < 3; i++) {
    tft.fillRect(centerX - 91, centerY + 34, 182, 14, TFT_WHITE);
    delay(50);
    tft.fillRoundRect(centerX - 90, centerY + 35, 180, 12, 6, COLOR_HEADER);
    tft.fillRect(centerX - 88, centerY + 37, 176, 8, COLOR_STATUS);
    delay(50);
  }
  
  tft.fillRect(0, tft.height() - 30, tft.width(), 30, COLOR_BACKGROUND);
  tft.fillRect(0, tft.height() - 30, tft.width(), 2, tft.color565(0, 100, 0));
  
  tft.setTextColor(COLOR_STATUS);
  tft.setTextSize(1);
  
  String successMsg = "Connection successful!";
  tft.setCursor(centerX - (successMsg.length() * 3), tft.height() - 25);
  for (int i = 0; i < successMsg.length(); i++) {
    tft.print(successMsg[i]);
    delay(20);
  }
  
  tft.setCursor(centerX - 60, tft.height() - 10);
  tft.setTextColor(COLOR_TEXT);
  tft.print(" IP: ");

  tft.setTextColor(COLOR_TIME);
  for (int i = 0; i < ipAddress.length(); i++) {
    tft.print(ipAddress[i]);
    delay(15);
  }
}

void wifiConnect() {
  displayWiFiConnectScreen();
  
  WiFi.begin(ssid, password);

  int attemptCount = 0;
  const int maxAttempts = 30;
  
  while (WiFi.status() != WL_CONNECTED && attemptCount < maxAttempts) {
    delay(300);
    Serial.print(".");
    
    int progressPercentage = map(attemptCount, 0, maxAttempts - 1, 0, 100);
    updateWiFiProgress(progressPercentage, attemptCount);
    
    attemptCount++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    displayWiFiSuccess(WiFi.localIP().toString());
    
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed");
    
    int centerX = tft.width() / 2;
    tft.fillRect(centerX - 100, tft.height() - 30, 200, 20, COLOR_BACKGROUND);
    tft.setCursor(centerX - 70, tft.height() - 30);
    tft.setTextColor(COLOR_WARNING);
    tft.print("Connection failed!");
    tft.setCursor(centerX - 90, tft.height() - 15);
    tft.print("Check WiFi credentials");
  }
  
  delay(2000);
}

String getCurrentDate() {
  struct tm timeinfo;
  char dateString[20];
  
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "Date not available";
  }
  
  strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo);
  return String(dateString);
}

String getCurrentTimeOnly() {
  struct tm timeinfo;
  char timeString[20];
  
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "Time not available";
  }
  
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  return String(timeString);
}

bool isTimeToCallAPI() {
  struct tm timeinfo;
  
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  
  if(timeinfo.tm_mday == lastCallDay) {
    return false;
  }
  
  if(timeinfo.tm_hour == API_CALL_HOUR && 
     timeinfo.tm_min == API_CALL_MINUTE && 
     timeinfo.tm_sec >= API_CALL_SECOND && 
     timeinfo.tm_sec < API_CALL_SECOND + 10) {
    lastCallDay = timeinfo.tm_mday;
    return true;
  }
  
  return false;
}

void displayTime() {
  tft.setRotation(3);
  
  tft.fillRect(0, 0, tft.width(), 20, COLOR_HEADER);
  tft.setCursor(5, 5);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  
  String timeStr = getCurrentTimeOnly();
  int timeWidth = timeStr.length() * 6;
  tft.setCursor(tft.width() - timeWidth - 5, 5);
  tft.setTextColor(COLOR_TIME);
  tft.print(timeStr);
  
  tft.setCursor(5, 5);
  tft.setTextColor(COLOR_STATUS);
  tft.print("Plant Monitor");
}

void displayFooter(bool isConnected) {
  int footerY = tft.height() - 20;
  tft.fillRect(0, footerY, tft.width(), 20, COLOR_HEADER);
  tft.setCursor(5, footerY + 5);
  tft.setTextSize(1);
  
  if (isConnected) {
    tft.setTextColor(COLOR_STATUS);
    tft.print("Online");
  } else {
    tft.setTextColor(COLOR_WARNING);
    tft.print("Offline");
  }
  
  tft.setCursor(tft.width() - 130, footerY + 5);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Next scan: 06:00 AM");
}

void showResultOnTFT(String payload) {
  tft.setRotation(3);
  
  int resultX = 160;
  int resultY = 23;
  int resultWidth = tft.width() - resultX - 5;
  int resultHeight = tft.height() - resultY - 23;
  
  tft.fillRect(resultX, resultY, resultWidth, resultHeight, COLOR_BACKGROUND);
  tft.drawRect(resultX, resultY, resultWidth, resultHeight, COLOR_BORDER);
  tft.drawRect(resultX + 1, resultY + 1, resultWidth - 2, resultHeight - 2, COLOR_BORDER);
  
  tft.fillRect(resultX + 2, resultY + 2, resultWidth - 4, 20, COLOR_HEADER);
  tft.setCursor(resultX + 10, resultY + 7);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);
  tft.print("API TEST RESULTS");
  
  int classStart = payload.indexOf("\"predicted_class\":") + 19;
  int classEnd = payload.indexOf("\"", classStart);
  String predictedClass = "Unknown";
  float confidence = 0.0;
  
  if (classStart > 19 && classEnd > classStart) {
    predictedClass = payload.substring(classStart, classEnd);
    
    int confStart = payload.indexOf("\"confidence\":") + 13;
    int confEnd = payload.indexOf(",", confStart);
    if (confStart > 13 && confEnd > confStart) {
      confidence = payload.substring(confStart, confEnd).toFloat() * 100;
    }
  } else {
    tft.setCursor(resultX + 10, resultY + 45);
    tft.setTextColor(COLOR_WARNING);
    tft.print("Error parsing response");
    
    if (payload.length() < 100) {
      tft.setCursor(resultX + 10, resultY + 65);
      tft.setTextColor(COLOR_TEXT);
      tft.print(payload);
    }
    return;
  }
  
  tft.setCursor(resultX + 10, resultY + 30);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Disease:");
  
  tft.setCursor(resultX + 10, resultY + 45);
  if (predictedClass != "Healthy") {
    tft.setTextColor(COLOR_WARNING);
  } else {
    tft.setTextColor(COLOR_STATUS);
  }
  tft.print(predictedClass);
  
  tft.setCursor(resultX + 10, resultY + 65);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Confidence: ");
  
  tft.setTextColor(COLOR_TIME);
  tft.print(int(confidence));
  tft.print("%");
  
  tft.setCursor(resultX + 10, resultY + 85);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Status: ");
  
  if (predictedClass != "Healthy") {
    tft.setTextColor(COLOR_WARNING);
    tft.print("DISEASE!");
  } else {
    tft.setTextColor(COLOR_STATUS);
    tft.print("HEALTHY");
  }
  
  tft.setCursor(resultX + 10, resultY + 105);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Date: ");
  tft.setTextColor(COLOR_TIME);
  tft.print(resultDate);
  
  tft.setCursor(resultX + 10, resultY + 120);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Time: ");
  tft.setTextColor(COLOR_TIME);
  tft.print(resultTime);
}

void sendImageToServer(camera_fb_t* fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    delay(1000);
    return;
  }
  
  HTTPClient http;
  
  http.begin(serverUrl);
  http.setTimeout(20000);

  http.addHeader("Content-Type", "multipart/form-data; boundary=diseasedetectionboundary");
  
  String head = "--diseasedetectionboundary\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--diseasedetectionboundary--\r\n";
  
  uint32_t imageLen = fb->len;
  uint32_t totalLen = head.length() + imageLen + tail.length();
  
  http.addHeader("Content-Length", String(totalLen));
  
  uint8_t *buffer = (uint8_t*)malloc(totalLen);
  if (buffer == NULL) {
    Serial.println("Not enough memory to send image");
    http.end();
    return;
  }
  
  uint32_t pos = 0;
  memcpy(buffer + pos, head.c_str(), head.length());
  pos += head.length();
  memcpy(buffer + pos, fb->buf, fb->len);
  pos += fb->len;
  memcpy(buffer + pos, tail.c_str(), tail.length());
  
  int httpCode = http.POST(buffer, totalLen);
  free(buffer);
  
  if (httpCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Server response:");
      Serial.println(payload);
      
      resultDate = getCurrentDate();
      resultTime = getCurrentTimeOnly();
      
      displayResults = true;
      resultText = payload;
      showResultOnTFT(payload);
      
      resultDisplayStartTime = millis();
      
      int classStart = payload.indexOf("\"predicted_class\":") + 19;
      int classEnd = payload.indexOf("\"", classStart);
      String predictedClass = "Unknown";
      if (classStart > 19 && classEnd > classStart) {
        predictedClass = payload.substring(classStart, classEnd);
      }
      sendDiseaseResultToESP32(predictedClass);
    }
    
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    tft.setTextColor(TFT_RED);
    tft.println("Failed to connect to server!");
    tft.println(http.errorToString(httpCode));
  }
  
  http.end();
}

void showingImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    Serial.println("Camera Capture Failed!");
  } else {
    tft.setRotation(0);
    TJpgDec.setJpgScale(2);
    Serial.println("Camera Image to Display");
    
    int offsetX = 40; // 40px
    int offsetY = 180; // 180px
    TJpgDec.drawJpg(offsetX, offsetY, (const uint8_t*)fb->buf, fb->len);
    displayTime();
    
    if(isTimeToCallAPI()) {
      Serial.println("It's time to call API at 6:00 AM!");
      sendImageToServer(fb);
    }
  }
  esp_camera_fb_return(fb);
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void displayInit() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BACKGROUND);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  
  tft.fillRect(0, 0, tft.width(), 20, COLOR_HEADER);
  tft.setCursor(5, 5);
  tft.setTextColor(COLOR_STATUS);
  tft.print("Plant Disease Monitor");
  
  tft.fillRoundRect(centerX-50, centerY-50, 100, 100, 10, TFT_DARKGREEN);
  tft.fillRoundRect(centerX-40, centerY-40, 80, 80, 5, TFT_GREEN);
  tft.drawLine(centerX, centerY-30, centerX, centerY+30, TFT_WHITE);
  tft.fillCircle(centerX-15, centerY-15, 10, TFT_RED);
  tft.fillCircle(centerX+20, centerY+10, 15, TFT_YELLOW);
  
  // Hiển thị thông báo khởi động
  tft.setCursor(centerX-60, centerY+70);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.println("Initializing...");

  int footerY = tft.height() - 20;
  tft.fillRect(0, footerY, tft.width(), 20, COLOR_HEADER);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
}

void testAPISend() {
  unsigned long currentTime = millis();
  
  // Kiểm tra nếu đã qua 10 giây kể từ lần test cuối
  if (enableAPITest && (currentTime - lastAPITest > 10000)) {
    lastAPITest = currentTime;
    
    // Hiển thị thông báo đang gửi
    tft.fillRect(5, 25, 150, 20, COLOR_BACKGROUND);
    tft.setCursor(5, 25);
    tft.setTextColor(COLOR_TIME);
    tft.print("Sending API test...");
    
    // Chụp ảnh
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_JPEG) {
      Serial.println("Camera capture failed");
      tft.setCursor(5, 25);
      tft.setTextColor(COLOR_WARNING);
      tft.print("Camera error!      ");
      return;
    }
    
    // Lưu ngày giờ hiện tại trước khi gửi API
    resultDate = getCurrentDate();
    resultTime = getCurrentTimeOnly();
    
    // Gửi hình ảnh tới API server
    sendImageToServer(fb);
    
    // Trả lại bộ nhớ
    esp_camera_fb_return(fb);
    
    // Xóa thông báo
    tft.fillRect(5, 25, 150, 20, COLOR_BACKGROUND);
  }
}

void sendDiseaseResultToESP32(const String& predictedClass) {
  HTTPClient http;
  http.begin(esp32Url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String postData = "predicted_class=" + predictedClass;
  int httpCode = http.POST(postData);
  Serial.printf("Send disease to ESP32, HTTP code: %d\n", httpCode);
  http.end();
}

void handlePredict() {
  Serial.println("Received request for disease detection from client");
  
  // Add CORS headers first
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // Handle preflight OPTIONS request
  if (server.method() == HTTP_OPTIONS) {
    server.send(200);
    return;
  }
  
  // Chụp ảnh từ camera
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    server.send(500, "application/json", "{\"error\":\"Camera capture failed\",\"timestamp\":\"" + getCurrentTimeOnly() + "\"}");
    Serial.println("Camera capture failed");
    return;
  }
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    server.send(500, "application/json", "{\"error\":\"WiFi not connected\",\"timestamp\":\"" + getCurrentTimeOnly() + "\"}");
    esp_camera_fb_return(fb);
    return;
  }
  
  HTTPClient http;
  
  // Ensure the API URL is correct
  Serial.print("Sending request to API URL: ");
  Serial.println(serverUrl);
  
  http.begin(serverUrl);
  http.setTimeout(20000); // Increase timeout to 20 seconds
  
  // Set header for multipart form data
  http.addHeader("Content-Type", "multipart/form-data; boundary=diseasedetectionboundary");
  
  // Create multipart form data
  String head = "--diseasedetectionboundary\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--diseasedetectionboundary--\r\n";
  
  uint32_t imageLen = fb->len;
  uint32_t totalLen = head.length() + imageLen + tail.length();
  
  http.addHeader("Content-Length", String(totalLen));
  
  // Log image size and remaining heap memory
  Serial.printf("Image size: %u bytes, Remaining heap: %u bytes\n", imageLen, ESP.getFreeHeap());
  
  uint8_t *buffer = (uint8_t*)malloc(totalLen);
  if (buffer == NULL) {
    server.send(500, "application/json", "{\"error\":\"Not enough memory to send image\",\"free_heap\":\"" + String(ESP.getFreeHeap()) + "\",\"timestamp\":\"" + getCurrentTimeOnly() + "\"}");
    http.end();
    esp_camera_fb_return(fb);
    return;
  }
  
  // Prepare data to send
  uint32_t pos = 0;
  memcpy(buffer + pos, head.c_str(), head.length());
  pos += head.length();
  memcpy(buffer + pos, fb->buf, fb->len);
  pos += fb->len;
  memcpy(buffer + pos, tail.c_str(), tail.length());
  
  // Send POST request and receive result
  Serial.println("Starting POST request...");
  int httpCode = http.POST(buffer, totalLen);
  Serial.printf("POST request sent, response code: %d\n", httpCode);
  
  free(buffer);
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String apiResult = http.getString();
      Serial.println("Response from prediction server:");
      Serial.println(apiResult);
      
      resultDate = getCurrentDate();
      resultTime = getCurrentTimeOnly();
      
      displayResults = true;
      resultText = apiResult;
      showResultOnTFT(apiResult);
      
      resultDisplayStartTime = millis();

      int classStart = apiResult.indexOf("\"predicted_class\":") + 19;
      int classEnd = apiResult.indexOf("\"", classStart);
      String predictedClass = "Unknown";
      if (classStart > 19 && classEnd > classStart) {
        predictedClass = apiResult.substring(classStart, classEnd);
      }
      sendDiseaseResultToESP32(predictedClass);
      
      server.send(200, "application/json", apiResult);
    } else {
      String errorMessage = "{\"error\":\"HTTP request failed with code: " + String(httpCode) + "\",\"timestamp\":\"" + getCurrentTimeOnly() + "\"}";
      server.send(httpCode, "application/json", errorMessage);
      Serial.printf("HTTP request error, error code: %d\n", httpCode);
    }
  } else {
    String errorMessage = "{\"error\":\"Connection to prediction server failed\",\"details\":\"" + http.errorToString(httpCode) + "\",\"timestamp\":\"" + getCurrentTimeOnly() + "\"}";
    server.send(500, "application/json", errorMessage);
    Serial.printf("Connection to prediction server failed: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  esp_camera_fb_return(fb);
}

void handleStatus() {
  // Add CORS headers
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // Handle preflight OPTIONS request
  if (server.method() == HTTP_OPTIONS) {
    server.send(200);
    return;
  }
  
  String statusJson = "{";
  statusJson += "\"status\":\"online\",";
  statusJson += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  statusJson += "\"ssid\":\"" + String(ssid) + "\",";
  
  // Get current time
  struct tm timeinfo;
  char timeStr[30] = "unknown";
  char dateStr[30] = "unknown";
  if (getLocalTime(&timeinfo)) {
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  }
  
  statusJson += "\"time\":\"" + String(timeStr) + "\",";
  statusJson += "\"date\":\"" + String(dateStr) + "\",";
  statusJson += "\"uptime\":" + String(millis() / 1000) + ",";
  statusJson += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  statusJson += "\"api_url\":\"" + String(serverUrl) + "\"";
  statusJson += "}";
  
  server.send(200, "application/json", statusJson);
}

// Thêm hàm xử lý yêu cầu chụp và trả về ảnh
void handleCaptureImage() {
  Serial.println("Received request for image capture from client");
  
  // Add CORS headers
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  
  // Handle preflight OPTIONS request
  if (server.method() == HTTP_OPTIONS) {
    server.send(200);
    return;
  }
  
  // Chụp ảnh từ camera
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    server.send(500, "text/plain", "Camera capture failed");
    Serial.println("Camera capture failed");
    return;
  }
  
  // Thiết lập headers
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.sendHeader("Connection", "close");
  // No need to add Access-Control-Allow-Origin again
  
  // Gửi hình ảnh về client
  server.setContentLength(fb->len);
  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);
  
  // Trả lại bộ nhớ và thông báo thành công
  esp_camera_fb_return(fb);
  Serial.println("Sent image to client");
}

// Thêm hàm để xử lý stream video MJPEG từ camera
void handleStream() {
  Serial.println("Received request for video stream from client");
  
  // Handle OPTIONS method directly for CORS preflight
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(200);
    return;
  }
  
  WiFiClient client = server.client();

  // Thiết lập headers cho MJPEG stream
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  // Send only one Access-Control-Allow-Origin header
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  
  // Biến đếm lỗi camera
  int captureErrors = 0;
  
  // Vòng lặp stream
  while (client.connected()) {
    // Chụp ảnh từ camera
    camera_fb_t* fb = esp_camera_fb_get();
    
    if (!fb || fb->format != PIXFORMAT_JPEG) {
      Serial.println("Camera capture failed for stream");
      captureErrors++;
      
      // If there are too many consecutive errors, exit
      if (captureErrors > 5) {
        break;
      }
      
      delay(1000);
      continue;
    }
    
    // Reset error counter
    captureErrors = 0;
    
    // Send frame header
    client.println();
    client.println("--frame");
    client.print("Content-Type: image/jpeg\r\n");
    client.print("Content-Length: ");
    client.println(fb->len);
    client.println();
    
    // Send image data
    client.write(fb->buf, fb->len);
    client.println();
    
    // Release memory
    esp_camera_fb_return(fb);
    
    // Process other server requests and wait a bit
    server.handleClient();
    delay(100);
  }
  
  Serial.println("Client disconnected from stream");
}

void setup() {
  Serial.begin(115200);
  
  displayInit();
  cameraInit();
  
  wifiConnect();
  
  // Display main screen
  tft.fillScreen(COLOR_BACKGROUND);
  
  // Display time
  displayTime();
  
  // Display footer
  displayFooter(WiFi.status() == WL_CONNECTED);
  
  // Set up the web server routes
  // server.on("/", HTTP_GET, handleRoot);           // Root page with control interface
  server.on("/predict", HTTP_GET, handlePredict);  // Handle GET method
  server.on("/predict", HTTP_POST, handlePredict); // Handle POST method
  server.on("/predict", HTTP_OPTIONS, handlePredict); // Handle OPTIONS requests for CORS
  
  server.on("/status", HTTP_GET, handleStatus);    // Handle GET method
  server.on("/status", HTTP_OPTIONS, handleStatus); // Handle OPTIONS requests for CORS
  
  server.on("/capture-image", HTTP_GET, handleCaptureImage); // Handle GET method
  server.on("/capture-image", HTTP_OPTIONS, handleCaptureImage); // Handle OPTIONS requests for CORS
  
  server.on("/stream", HTTP_GET, handleStream); // Handle GET method
  server.on("/stream", HTTP_OPTIONS, handleStream); // Handle OPTIONS requests for CORS
  
  server.begin();
  
  Serial.println("Web server is ready - access:");
  // Serial.println("- http://" + WiFi.localIP().toString() + "/");
  Serial.println("- http://" + WiFi.localIP().toString() + "/predict");
  Serial.println("- http://" + WiFi.localIP().toString() + "/status");
  Serial.println("- http://" + WiFi.localIP().toString() + "/capture-image");
  Serial.println("- http://" + WiFi.localIP().toString() + "/stream");
  
  delay(1000);
  
  // Reset the API test time counter
  lastAPITest = millis();
}

void loop() {
  // Process Web Server requests
  server.handleClient();
  
  // Update time
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 1000) {
    displayTime();
    lastTimeUpdate = millis();
  }
  
  // Stream camera always running, not dependent on displayResults
  showingImage();
  
  // If there is a result to display, draw the result on the screen after the camera has been displayed
  if (displayResults && !resultText.isEmpty()) {
    showResultOnTFT(resultText);
    
    // Check if the result has been displayed long enough to clear it
    if (millis() - resultDisplayStartTime > RESULT_DISPLAY_DURATION) {
      // Only clear the result display, not affecting the camera part
      int resultX = 160;
      int resultY = 23;
      int resultWidth = tft.width() - resultX - 5;
      int resultHeight = tft.height() - resultY - 23;
      tft.fillRect(resultX, resultY, resultWidth, resultHeight, COLOR_BACKGROUND);
      
      // Reset the result display variable
      displayResults = false;
      resultText = "";
    }
  }
}