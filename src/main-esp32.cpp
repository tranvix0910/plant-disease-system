#include "Arduino.h"
#include "config.h"
#include "reports.h"
#include "water_pump.h"
#include "tele_bot.h"
#include "web_server.h"
#include "handleWifi.h"
#include "dht11.h"
#include "soilmoisture.h"
#include "gemini.h"
#include "google_sheets_service.h"

const long gmtOffset_sec = 7 * 3600; // UTC+7
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

const unsigned long SEND_DATA_INTERVAL = 5 * 60 * 1000;
unsigned long lastSendTime = 0;

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo cảm biến DHT
  dht11Setup();

  // Configure sensor pins and control
  soilMoistureSetup();
  waterPumpSetup();
  
  connectToWifi();
  
  telegramBotSetup();
  
  webServerSetup();
  
  // Cấu hình thời gian từ NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Đợi đồng bộ thời gian
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Lỗi lấy thời gian từ Internet!");
  } else {
    Serial.println("Đã đồng bộ thời gian: " + String(timeinfo.tm_hour) + ":" + 
                  String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
  }
  
  sendDataToGoogleSheets();

  getWeatherAndAskGemini();
  
  // Thông báo hệ thống đã sẵn sàng
  String startupMsg = "🚀 *HỆ THỐNG ĐÃ KHỞI ĐỘNG* 🚀\n\n";
  startupMsg += "✅ Kết nối WiFi thành công\n";
  startupMsg += "✅ Đồng bộ thời gian thành công\n";
  startupMsg += "✅ Gửi dữ liệu lên Google Sheets đã sẵn sàng\n";
  startupMsg += "✅ Dự báo thời tiết và phân tích đã hoạt động\n\n";
  startupMsg += "Hệ thống sẽ tự động gửi dữ liệu mỗi 5 phút và báo cáo hàng ngày lúc 23:00.";
  
  sendToAllChannels(startupMsg, "Markdown");
}


void loop() {
  server.handleClient();
  
  checkAndWater();
  
  checkAndStopPump();
  
  unsigned long currentMillis = millis();
  
  // Add sensor reading every 5 seconds
  static unsigned long lastSensorReadTime = 0;
  if (currentMillis - lastSensorReadTime >= 5000) {
    lastSensorReadTime = currentMillis;
    
    // Read sensor data
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
    float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    
    // Update global variables for webserver access
    latestTemperature = temperature;
    latestHumidity = humidity;
    latestSoilMoisturePercent = soilMoisturePercent;
    
    // Print to serial monitor
    Serial.println("===== Sensor Data (5s) =====");
    Serial.print("Temperature: "); 
    Serial.print(temperature); 
    Serial.println(" °C");
    Serial.print("Humidity: "); 
    Serial.print(humidity); 
    Serial.println(" %");
    Serial.print("Soil Moisture ADC: ");
    Serial.println(soilMoistureValue);
    Serial.print("Soil Moisture: ");
    Serial.print(soilMoisturePercent);
    Serial.println(" %");
    Serial.println("===========================");
  }
  
  if (currentMillis - lastSendTime >= SEND_DATA_INTERVAL) {
    lastSendTime = currentMillis;
    sendDataToGoogleSheets();
  }
  
  // Kiểm tra xem có đến thời gian gửi báo cáo hàng ngày chưa
  if (isTimeToSendDailyReport()) {
    sendDailyReport();
  }
  
  // Kiểm tra tin nhắn Telegram
  messageCheck();
}