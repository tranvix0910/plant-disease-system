#include "water_pump.h"

unsigned long wateringStartTime = 0;

void waterPumpSetup() {
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, LOW);
}

int waterPumpRead() {
    return digitalRead(WATER_PUMP_PIN);
}

void startWaterPump() {
  digitalWrite(WATER_PUMP_PIN, HIGH);
  wateringStartTime = millis();
  
  struct tm timeinfo;
  char currentTimeStr[9]; // HH:MM:SS\0
  
  if (getLocalTime(&timeinfo)) {
    strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M:%S", &timeinfo);
  } else {
    strcpy(currentTimeStr, "Không xác định");
  }
  
    String message = "💧 *BẮT ĐẦU TƯỚI NƯỚC TỰ ĐỘNG* 💧\n\n";
  message += "⏰ *Thời gian bắt đầu tưới*: " + String(currentTimeStr) + "\n";
  message += "⏱️ *Thời lượng tưới*: " + String(PUMP_DURATION / 1000) + " giây\n";
  
  sendToAllChannels(message, "Markdown");
  
  Serial.println("Bắt đầu tưới nước tự động, thời gian hiện tại: " + String(currentTimeStr));
}

bool checkAndWater() {

  if (!wateringScheduleActive || scheduledWateringTime.length() == 0) {
    return false;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Không thể lấy thời gian hiện tại");
    return false;
  }
  
  char currentTimeStr[6]; // HH:MM\0
  strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M", &timeinfo);
  String currentTime = String(currentTimeStr);
  
  char currentDateStr[11]; // YYYY-MM-DD\0
  strftime(currentDateStr, sizeof(currentDateStr), "%Y-%m-%d", &timeinfo);
  static String lastWateringDate = "";
  
  if (lastWateringDate != String(currentDateStr)) {
    lastWateringDate = String(currentDateStr);
    alreadyWateredToday = false;
  }
  
  if (currentTime == scheduledWateringTime && !alreadyWateredToday) {
    int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
    float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    
    if (soilMoistureValue > SOIL_THRESHOLD || soilMoisturePercent < 35) {
      startWaterPump();
      return true;
    } else {
      String message = "🌧️ *KIỂM TRA TƯỚI NƯỚC TỰ ĐỘNG* 🌧️\n\n";
      message += "⏰ *Thời gian kiểm tra*: " + currentTime + "\n";
      message += "🌱 *Độ ẩm đất hiện tại*: " + String(soilMoisturePercent, 0) + "%\n\n";
      message += "✅ Độ ẩm đất đủ cao, không cần tưới nước.\n";
      
      sendToAllChannels(message, "Markdown");
      
      alreadyWateredToday = true;
      return false;
    }
  }
  
  return false;
}

void checkAndStopPump() {
  if (digitalRead(WATER_PUMP_PIN) == HIGH) {
    if (millis() - wateringStartTime >= PUMP_DURATION) {

      digitalWrite(WATER_PUMP_PIN, LOW);
      
        // Đánh dấu đã tưới nước hôm nay
      alreadyWateredToday = true;
      
      // Lấy thời gian hiện tại
      struct tm timeinfo;
      char currentTimeStr[9]; // HH:MM:SS\0
      
      if (getLocalTime(&timeinfo)) {
        strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M:%S", &timeinfo);
      } else {
        strcpy(currentTimeStr, "Không xác định");
      }
      
      // Đọc độ ẩm đất sau khi tưới
      int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
      float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
      soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
      
      String message = "✅ *HOÀN THÀNH TƯỚI NƯỚC TỰ ĐỘNG* ✅\n\n";
      message += "⏰ *Thời gian kết thúc*: " + String(currentTimeStr) + "\n";
      message += "⏱️ *Thời lượng đã tưới*: " + String(PUMP_DURATION / 1000) + " giây\n";
      message += "🌱 *Độ ẩm đất sau khi tưới*: " + String(soilMoisturePercent, 0) + "%\n";
      
      sendToAllChannels(message, "Markdown");
      
      Serial.println("Kết thúc tưới nước tự động, thời gian: " + String(currentTimeStr));
    }
  }
}