#include "google_sheets_service.h"

void sendDataToGoogleSheets() {
  float temperature = readTemperature();
  float humidity = readHumidity();
  
  float soilMoisturePercent = readSoilMoisturePercent();
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Lỗi đọc dữ liệu DHT11!");
    return;
  }

  Serial.println("===== Dữ liệu cảm biến =====");
  Serial.print("Nhiệt độ: "); 
  Serial.print(temperature); 
  Serial.println(" °C");
  Serial.print("Độ ẩm không khí: "); 
  Serial.print(humidity); 
  Serial.println(" %");
  Serial.print("Độ ẩm đất: ");
  Serial.print(soilMoisturePercent);
  Serial.println(" %");
  
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Không thể lấy thời gian");
    return;
  }
  
  char dateStr[11];
  char timeStr[9];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  
  String urlFinal = "https://script.google.com/macros/s/" + String(GOOGLE_SCRIPT_ID) + "/exec?";
  urlFinal += "date=" + String(dateStr);
  urlFinal += "&time=" + String(timeStr);
  urlFinal += "&temperature=" + String(temperature);
  urlFinal += "&humidity=" + String(humidity);
  urlFinal += "&soil_moisture=" + String(soilMoisturePercent);
  
  Serial.print("Gửi dữ liệu lên Google Sheets: ");
  Serial.println(urlFinal);
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, urlFinal);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    
    String payload = http.getString();
    Serial.println("Phản hồi HTTP code: " + String(httpCode));
    Serial.println("Nội dung phản hồi: " + payload);
    
    // Thông báo qua Telegram
    String message = "📊 *CẬP NHẬT DỮ LIỆU* 📊\n\n";
    message += "🕒 *Thời gian*: " + String(dateStr) + " " + String(timeStr) + "\n";
    message += "🌡️ *Nhiệt độ*: " + String(temperature) + " °C\n";
    message += "💧 *Độ ẩm không khí*: " + String(humidity) + " %\n";
    message += "🌱 *Độ ẩm đất*: " + String(soilMoisturePercent) + " %\n\n";
    message += "✅ Đã cập nhật lên Google Sheets!";
    
    sendToAllChannels(message, "Markdown");
  } else {
    Serial.println("Lỗi kết nối HTTP, code: " + String(httpCode));
    Serial.println("Chi tiết lỗi: " + http.errorToString(httpCode));
    sendToAllChannels("❌ Lỗi cập nhật dữ liệu lên Google Sheets! Code: " + String(httpCode), "");
  }
  
  http.end();
}

void testGoogleScriptConnection() {
  Serial.println("Kiểm tra kết nối đến Google Script...");
  
  String urlFinal = "https://script.google.com/macros/s/" + String(GOOGLE_SCRIPT_ID) + "/exec?";
  urlFinal += "action=test";
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, urlFinal);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  
  Serial.print("Gửi yêu cầu kiểm tra: ");
  Serial.println(urlFinal);
  
  int httpCode = http.GET();
  Serial.print("HTTP Code: ");
  Serial.println(httpCode);
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Nội dung phản hồi: " + payload);
    
    // Kiểm tra xem nội dung có phải là JSON hợp lệ không
    if (payload.startsWith("{")) {
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
        message += "✅ Kết nối đến Google Script thành công!\n";
        message += "📊 HTTP code: " + String(httpCode) + "\n";
        if (doc.containsKey("status")) {
          message += "🔹 Trạng thái: " + doc["status"].as<String>() + "\n";
        }
        if (doc.containsKey("message")) {
          message += "🔹 Thông báo: " + doc["message"].as<String>() + "\n";
        }
        if (doc.containsKey("timestamp")) {
          message += "🔹 Thời gian phản hồi: " + doc["timestamp"].as<String>() + "\n";
        }
        
        sendToAllChannels(message, "Markdown");
      } else {
        String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
        message += "❌ Kết nối đến Google Script không thành công!\n";
        message += "📊 HTTP code: " + String(httpCode) + "\n";
        message += "❓ Lỗi: Không thể phân tích phản hồi JSON\n";
        message += "📜 Phản hồi: " + payload;
        
        sendToAllChannels(message, "Markdown");
      }
    } else {
      String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
      message += "⚠️ Kết nối đến Google Script không bình thường!\n";
      message += "📊 HTTP code: " + String(httpCode) + "\n";
      message += "❓ Lỗi: Phản hồi không phải là JSON\n";
      message += "📜 Phản hồi: " + payload;
      
      sendToAllChannels(message, "Markdown");
    }
  } else {
    String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
    message += "❌ Không thể kết nối đến Google Script!\n";
    message += "📊 HTTP code: " + String(httpCode) + "\n";
    message += "❓ Lỗi: " + http.errorToString(httpCode);
    
    sendToAllChannels(message, "Markdown");
  }
  
  http.end();
}