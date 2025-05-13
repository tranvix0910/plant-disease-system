#include "esp_camera.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

// Cấu hình Wi-Fi
// const char* ssid = "311HHN Lau 1";
// const char* password = "@@1234abcdlau1";
const char* ssid = "AndroidAP9B0A";
const char* password = "quynhquynh";

// Địa chỉ API server
const char* serverUrl = "http://172.16.10.140:8000/predict/";

// Cấu hình thời gian   
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;
const int daylightOffset_sec = 0;

// Định nghĩa thời gian gọi API
const int API_CALL_HOUR = 6;   // 6 giờ sáng
const int API_CALL_MINUTE = 0; // 0 phút
const int API_CALL_SECOND = 0; // 0 giây

// Định nghĩa chân GPIO
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
bool apiCallToday = false; // Biến để theo dõi đã gọi API hôm nay chưa
int lastCallDay = -1;      // Ngày cuối cùng đã gọi API

#define COLOR_BACKGROUND  TFT_BLACK
#define COLOR_HEADER      TFT_NAVY
#define COLOR_TEXT        TFT_WHITE
#define COLOR_TIME        TFT_CYAN
#define COLOR_STATUS      TFT_GREEN
#define COLOR_WARNING     TFT_RED
#define COLOR_BORDER      TFT_DARKGREY

bool enableAPITest = true;
unsigned long lastAPITest = 0;

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
  config.frame_size = FRAMESIZE_QCIF; // 160x120
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
  // Đảm bảo rotation đúng là 3
  tft.setRotation(3);
  
  // Xóa toàn bộ màn hình với hiệu ứng gradient từ trên xuống dưới
  for (int y = 0; y < tft.height(); y += 4) {
    uint16_t gradientColor = tft.color565(0, 0, map(y, 0, tft.height(), 0, 40));
    tft.fillRect(0, y, tft.width(), 4, gradientColor);
    delay(2); // Tạo hiệu ứng từ từ
  }
  
  // Vẽ header đẹp mắt với viền và gradient
  tft.fillRoundRect(0, 0, tft.width(), 25, 5, COLOR_HEADER);
  tft.fillRect(0, 0, tft.width(), 20, COLOR_HEADER);
  tft.fillRect(0, 20, tft.width(), 5, tft.color565(0, 0, 80)); // Viền đậm ở dưới
  
  // Hiệu ứng tiêu đề
  tft.setCursor(5, 5);
  tft.setTextColor(COLOR_TIME);
  tft.setTextSize(1);
  tft.print("WIRELESS");
  tft.setCursor(70, 5);
  tft.setTextColor(COLOR_TEXT);
  tft.print("NETWORK SETUP");
  
  // Hiển thị tên mạng WiFi
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2 - 30;
  
  // Vẽ khung chứa thông tin WiFi hiệu ứng đẹp
  for (int i = 0; i < 5; i++) {
    tft.drawRoundRect(centerX - 110 - i, centerY - 25 - i, 220 + i*2, 95 + i*2, 10, tft.color565(0, 40 + i*10, 80 + i*10));
    delay(30); // Hiệu ứng dần dần hiện ra
  }
  tft.fillRoundRect(centerX - 110, centerY - 25, 220, 95, 10, COLOR_HEADER);
  
  // Tạo hiệu ứng gradiant trong khung
  for (int i = 0; i < 90; i++) {
    uint16_t gradColor = tft.color565(0, 0, map(i, 0, 90, 50, 120));
    tft.drawRoundRect(centerX - 105 + i/2, centerY - 20 + i/2, 210 - i, 85 - i, 8, gradColor);
    if (i % 15 == 0) delay(5);
  }
  
  // Hiển thị thông tin WiFi
  tft.setCursor(centerX - 80, centerY - 5);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.print("Connecting to Network:");
  
  // Hiệu ứng hiển thị SSID từng ký tự
  tft.setCursor(centerX - 80, centerY + 15);
  tft.setTextColor(COLOR_TIME);
  tft.setTextSize(1);
  for (int i = 0; i < strlen(ssid); i++) {
    tft.print(ssid[i]);
    delay(20);
  }
  
  // Vẽ thanh tiến trình đẹp hơn
  tft.drawRoundRect(centerX - 90, centerY + 35, 180, 12, 6, COLOR_BORDER);
  tft.drawRoundRect(centerX - 91, centerY + 34, 182, 14, 7, tft.color565(100, 100, 100));
  
  // Vẽ viền sáng và tối cho thanh tiến trình để tạo hiệu ứng 3D
  tft.drawFastHLine(centerX - 90, centerY + 35, 180, tft.color565(150, 150, 150));
  tft.drawFastVLine(centerX - 90, centerY + 35, 12, tft.color565(150, 150, 150));
  tft.drawFastHLine(centerX - 90, centerY + 46, 180, tft.color565(50, 50, 50));
  tft.drawFastVLine(centerX + 89, centerY + 35, 12, tft.color565(50, 50, 50));
  
  // Thêm phần chân trang với gradient
  tft.fillRect(0, tft.height() - 30, tft.width(), 30, COLOR_BACKGROUND);
  tft.fillRect(0, tft.height() - 30, tft.width(), 2, tft.color565(60, 60, 60));
  
  // Hiển thị thông báo phía dưới
  tft.setCursor(centerX - 95, tft.height() - 20);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.print("Connecting to network");
  
  // Hiệu ứng ba dấu chấm
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
  
  // Cập nhật thanh tiến trình với hiệu ứng gradient
  int progressWidth = map(progressPercentage, 0, 100, 0, 176);
  
  // Vẽ gradient cho thanh tiến trình
  for (int i = 0; i < progressWidth; i++) {
    uint16_t progressColor = tft.color565(
      map(i, 0, 176, 0, 100),  // R tăng dần
      map(i, 0, 176, 150, 255), // G tăng dần
      map(i, 0, 176, 100, 200)  // B tăng dần
    );
    tft.drawFastVLine(centerX - 88 + i, centerY + 37, 8, progressColor);
  }
  
  // Vẽ đường viền sáng ở trên cùng của phần đã hoàn thành
  tft.drawFastHLine(centerX - 88, centerY + 37, progressWidth, tft.color565(200, 255, 255));
  
  // Cập nhật dòng trạng thái với hiệu ứng
  tft.fillRect(centerX - 95, tft.height() - 20, 190, 15, COLOR_BACKGROUND);
  tft.setCursor(centerX - 95, tft.height() - 20);
  
  // Thay đổi màu sắc văn bản theo tiến trình
  uint16_t textColor = tft.color565(
    200,
    map(progressPercentage, 0, 100, 200, 255),
    map(progressPercentage, 0, 100, 200, 255)
  );
  tft.setTextColor(textColor);
  
  String statusText = "Searching for network";
  tft.print(statusText);
  
  // Hiệu ứng dấu chấm động
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
  
  // Hiển thị phần trăm tiến trình
  tft.fillRect(centerX + 60, tft.height() - 20, 40, 15, COLOR_BACKGROUND);
  tft.setCursor(centerX + 60, tft.height() - 20);
  tft.setTextColor(COLOR_TIME);
  tft.print(progressPercentage);
  tft.print("%");
}

void displayWiFiSuccess(String ipAddress) {
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2 - 30;
  
  // Hoàn thành thanh tiến trình với hiệu ứng đẹp
  for (int i = 0; i < 176; i++) {
    uint16_t progressColor = tft.color565(0, 255, map(i, 0, 176, 100, 150));
    tft.drawFastVLine(centerX - 88 + i, centerY + 37, 8, progressColor);
    if (i % 20 == 0) delay(5); // Hiệu ứng dần dần
  }
  
  // Hiệu ứng flash nhanh màn hình để chỉ báo kết nối thành công
  for (int i = 0; i < 3; i++) {
    tft.fillRect(centerX - 91, centerY + 34, 182, 14, TFT_WHITE);
    delay(50);
    tft.fillRoundRect(centerX - 90, centerY + 35, 180, 12, 6, COLOR_HEADER);
    tft.fillRect(centerX - 88, centerY + 37, 176, 8, COLOR_STATUS);
    delay(50);
  }
  
  // Hiển thị thông báo đã kết nối thành công
  tft.fillRect(0, tft.height() - 30, tft.width(), 30, COLOR_BACKGROUND);
  tft.fillRect(0, tft.height() - 30, tft.width(), 2, tft.color565(0, 100, 0));
  
  // Văn bản kết nối thành công với hiệu ứng xuất hiện dần
  tft.setTextColor(COLOR_STATUS);
  tft.setTextSize(1);
  
  // Dòng trạng thái thành công - xuất hiện từng ký tự
  String successMsg = "Connection successful!";
  tft.setCursor(centerX - (successMsg.length() * 3), tft.height() - 25);
  for (int i = 0; i < successMsg.length(); i++) {
    tft.print(successMsg[i]);
    delay(20);
  }
  
  // Hiển thị địa chỉ IP với hiệu ứng
  tft.setCursor(centerX - 60, tft.height() - 10);
  tft.setTextColor(COLOR_TEXT);
  tft.print(" IP: ");
  
  // Hiển thị IP từng ký tự với màu khác
  tft.setTextColor(COLOR_TIME);
  for (int i = 0; i < ipAddress.length(); i++) {
    tft.print(ipAddress[i]);
    delay(15);
  }
}

void wifiConnect() {
  // Hiển thị màn hình kết nối WiFi
  displayWiFiConnectScreen();
  
  // Bắt đầu kết nối WiFi
  WiFi.begin(ssid, password);

  int attemptCount = 0;
  const int maxAttempts = 30; // Giới hạn số lần thử kết nối
  
  while (WiFi.status() != WL_CONNECTED && attemptCount < maxAttempts) {
    delay(300);
    Serial.print(".");
    
    // Cập nhật màn hình tiến trình
    int progressPercentage = map(attemptCount, 0, maxAttempts - 1, 0, 100);
    updateWiFiProgress(progressPercentage, attemptCount);
    
    attemptCount++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Hiển thị thông báo kết nối thành công
  Serial.println("");
  Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Cập nhật UI thành công
    displayWiFiSuccess(WiFi.localIP().toString());
    
    // Cấu hình thời gian
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    // Xử lý trường hợp không kết nối được
    Serial.println("");
    Serial.println("WiFi connection failed");
    
    // Hiển thị thông báo lỗi kết nối
    int centerX = tft.width() / 2;
    tft.fillRect(centerX - 100, tft.height() - 30, 200, 20, COLOR_BACKGROUND);
    tft.setCursor(centerX - 70, tft.height() - 30);
    tft.setTextColor(COLOR_WARNING);
    tft.print("Connection failed!");
    tft.setCursor(centerX - 90, tft.height() - 15);
    tft.print("Check WiFi credentials");
  }
  
  // Dừng lại một chút để người dùng có thể thấy kết quả
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
  
  // Vẽ header bar
  tft.fillRect(0, 0, tft.width(), 20, COLOR_HEADER);
  tft.setCursor(5, 5);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  
  // Hiển thị thời gian
  String timeStr = getCurrentTimeOnly();
  int timeWidth = timeStr.length() * 6; // Ước tính độ rộng của chuỗi thời gian (6 pixel cho mỗi ký tự)
  tft.setCursor(tft.width() - timeWidth - 5, 5);
  tft.setTextColor(COLOR_TIME);
  tft.print(timeStr);
  
  // Hiển thị trạng thái hệ thống
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
  
  // Hiển thị giờ kế tiếp sẽ gửi ảnh
  tft.setCursor(tft.width() - 130, footerY + 5);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Next scan: 06:00 AM");
}

void showResultOnTFT(String payload) {
  // Không thay đổi rotation
  
  // Vẽ khung kết quả (bên phải ảnh camera)
  int resultX = 160;
  int resultY = 23; // Cách header 3px
  int resultWidth = tft.width() - resultX - 10;
  int resultHeight = tft.height() - resultY - 23; // Cách footer 3px
  
  // Vẽ khung chứa kết quả phân tích
  tft.fillRect(resultX, resultY, resultWidth, resultHeight, COLOR_BACKGROUND);
  tft.drawRect(resultX, resultY, resultWidth, resultHeight, COLOR_BORDER);
  tft.drawRect(resultX + 1, resultY + 1, resultWidth - 2, resultHeight - 2, COLOR_BORDER);
  
  // Hiển thị tiêu đề kết quả
  tft.fillRect(resultX + 2, resultY + 2, resultWidth - 4, 20, COLOR_HEADER);
  tft.setCursor(resultX + 10, resultY + 7);
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);
  tft.print("API TEST RESULTS");
  
  // Phân tích JSON
  int classStart = payload.indexOf("\"predicted_class\":") + 19;
  int classEnd = payload.indexOf("\"", classStart);
  String predictedClass = payload.substring(classStart, classEnd);
  
  int confStart = payload.indexOf("\"confidence\":") + 13;
  int confEnd = payload.indexOf(",", confStart);
  float confidence = payload.substring(confStart, confEnd).toFloat() * 100;
  
  // Hiển thị thông tin phân tích
  tft.setCursor(resultX + 10, resultY + 30);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Disease:");
  
  // Hiển thị kết quả với màu sắc phù hợp
  tft.setCursor(resultX + 10, resultY + 45);
  if (predictedClass != "Healthy") {
    tft.setTextColor(COLOR_WARNING);
  } else {
    tft.setTextColor(COLOR_STATUS);
  }
  tft.print(predictedClass);
  
  // Hiển thị độ tin cậy
  tft.setCursor(resultX + 10, resultY + 65);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Confidence: ");
  
  tft.setTextColor(COLOR_TIME);
  tft.print(int(confidence));
  tft.print("%");
  
  // Hiển thị trạng thái
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
  
  // Hiển thị thời gian cập nhật chia làm 2 hàng: ngày và giờ
  tft.setCursor(resultX + 10, resultY + 105);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Date: ");
  tft.setTextColor(COLOR_TIME);
  tft.print(getCurrentDate());
  
  tft.setCursor(resultX + 10, resultY + 120);
  tft.setTextColor(COLOR_TEXT);
  tft.print("Time: ");
  tft.setTextColor(COLOR_TIME);
  tft.print(getCurrentTimeOnly());
}

void sendImageToServer(camera_fb_t* fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    delay(1000);
    return;
  }
  
  HTTPClient http;
  
  http.begin(serverUrl);
  
  // Specify content type for multipart form data
  http.addHeader("Content-Type", "multipart/form-data; boundary=diseasedetectionboundary");
  
  // Create multipart form data
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
      
      // Display results on TFT
      displayResults = true;
      resultText = payload;
      showResultOnTFT(payload);
    }
  } else {
    Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    tft.setTextColor(TFT_RED);
    tft.println("Failed to connect to server!");
    tft.println(http.errorToString(httpCode));
  }
  
  http.end();
  delay(3000);
  displayResults = false;
}

void showingImage() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->format != PIXFORMAT_JPEG) {
    Serial.println("Camera Capture Failed!");
  } else {
    tft.setRotation(0);
    Serial.println("Camera Image to Display");
    
    int offsetX = 32; // 40px
    int offsetY = 168; // 180px
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
    
    // Gửi hình ảnh tới API server
    sendImageToServer(fb);
    
    // Trả lại bộ nhớ
    esp_camera_fb_return(fb);
    
    // Xóa thông báo
    tft.fillRect(5, 25, 150, 20, COLOR_BACKGROUND);
  }
}

void setup() {
  Serial.begin(115200);
  
  displayInit();
  cameraInit();
  
  // Kết nối WiFi với giao diện đồng bộ
  wifiConnect();
  
  // Hiển thị màn hình chính
  tft.fillScreen(COLOR_BACKGROUND);
  
  // Hiển thị thời gian
  displayTime();
  
  // Hiển thị footer
  displayFooter(WiFi.status() == WL_CONNECTED);
  
  delay(1000);
  
  // Reset bộ đếm thời gian test API
  lastAPITest = millis();
}

void loop() {
  // Cập nhật thời gian
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 1000) {
    displayTime();
    lastTimeUpdate = millis();
  }
  
  // Test API nếu được bật
  // testAPISend();
  
  // Hiển thị hình ảnh từ camera khi không đang hiển thị kết quả
  if (!displayResults) {
    showingImage();
  }
}