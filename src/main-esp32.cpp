#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>
#include <time.h>
#include <WebServer.h>
#include "user_interface.h"

// Định nghĩa chân cảm biến
#define DHTPIN 18     // Chân kết nối DHT11
#define DHTTYPE DHT11 // Loại cảm biến DHT
#define SOIL_MOISTURE_PIN 34 // Chân kết nối cảm biến độ ẩm đất (ADC)
#define WATER_PUMP_PIN 5    // Chân điều khiển bơm nước

// Ngưỡng độ ẩm đất
#define DRY_SOIL 3500   // Giá trị ADC khi đất khô
#define WET_SOIL 1500   // Giá trị ADC khi đất ẩm
#define SOIL_THRESHOLD 2800 // Ngưỡng cần tưới nước 

// Thời gian hoạt động của bơm nước (mili giây)
#define PUMP_DURATION 10000 // 10 giây

// Khai báo cảm biến
DHT dht(DHTPIN, DHTTYPE);

// const char* ssid = "311HHN Lau 1";
// const char* password = "@@1234abcdlau1";
// const char* ssid = "AndroidAP9B0A";
// const char* password = "quynhquynh";
const char* ssid = "Thai Bao";
const char* password = "0869334749";


const char* Gemini_Token = "AIzaSyA3ogt7LgUlDTuHqtMPZsFFompKnuYADAw";
const char* Gemini_Max_Tokens = "10000";
#define BOTtoken "7729298728:AAGwPQvhVE8sc9FlNHDDSLqUU8WLVzt-0QU"
#define CHAT_ID_1 "5797970828"
#define CHAT_ID_2 "1281777025"

String GOOGLE_SCRIPT_ID = "AKfycbyc35lHlrtRBJuDLHe6S0J6tfLUoeXDdRCNGCn1xfNODKvugb28w5pMcGAAuQcMT8ShWA"; 
// https://script.google.com/macros/s/AKfycbyQDiyTUR2SpwHikvLMXQDJ478LS1SOPTMyiO9TWAQIrVImQNv2Me5f_MhxkdaUmaGPEg/exec

// Tọa độ TP. Hồ Chí Minh
const char* latitude = "10.7769";
const char* longitude = "106.7009";
const char* timezone = "Asia/Ho_Chi_Minh";
char reason[10000];

// Thiết lập thời gian
const long gmtOffset_sec = 7 * 3600; // UTC+7
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

// Thời gian giữa các lần gửi dữ liệu (5 phút)
const unsigned long SEND_DATA_INTERVAL = 5 * 60 * 1000;
unsigned long lastSendTime = 0;

// Thời gian gửi báo cáo hàng ngày (mặc định 23:00)
int dailyReportHour = 23;
int dailyReportMinute = 0;
bool reportSentToday = false;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int botRequestDelay = 1000;
unsigned long lastTimeBotRan;
const long messageInterval = 5000; // 5 giây
unsigned long lastMessageTime = 0;

// Biến lưu thông tin bệnh từ ESP32-CAM
String predictedDisease = "Không có";
unsigned long lastDiseaseUpdateTime = 0;

// Khởi tạo WebServer để nhận dữ liệu bệnh từ ESP32-CAM
WebServer server(80);

// Biến lưu thời gian tưới nước từ Gemini
String scheduledWateringTime = ""; // Định dạng "HH:MM"
bool wateringScheduleActive = false;
bool alreadyWateredToday = false;
unsigned long wateringStartTime = 0;

// Global variables to store the latest sensor readings
float latestTemperature = 0;
float latestHumidity = 0;
float latestSoilMoisturePercent = 0;

// Add this global variable to store the latest analysis results
String lastAnalysisResults = "";
bool analysisInProgress = false;
unsigned long analysisStartTime = 0;

// Add global variables to store report data similar to analysis data
String lastReportResults = "";
bool reportInProgress = false;
unsigned long reportStartTime = 0;

// Function to handle the API update endpoint
void handleUpdate() {
  // Read sensor data
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  
  // Update global variables
  latestTemperature = temperature;
  latestHumidity = humidity;
  latestSoilMoisturePercent = soilMoisturePercent;
  
  // Create JSON response
  String jsonResponse = "{";
  jsonResponse += "\"temperature\":" + String(temperature, 1) + ",";
  jsonResponse += "\"humidity\":" + String(humidity, 1) + ",";
  jsonResponse += "\"soil_moisture\":" + String(soilMoisturePercent, 1) + ",";
  jsonResponse += "\"pump_status\":\"" + String(digitalRead(WATER_PUMP_PIN) == HIGH ? "on" : "off") + "\",";
  jsonResponse += "\"next_watering_time\":\"" + scheduledWateringTime + "\"";
  jsonResponse += "}";
  
  // Send response
  server.send(200, "application/json", jsonResponse);
  
  Serial.println("Data updated via web interface");
}



void handleRoot() {
  // Use the latest sensor readings that are updated every 5 seconds
  String html = String(PLANT_MONITOR_HTML);
  html.replace("{{TEMPERATURE}}", isnan(latestTemperature) ? "--" : String(latestTemperature, 1));
  html.replace("{{HUMIDITY}}", isnan(latestHumidity) ? "--" : String(latestHumidity, 1));
  html.replace("{{SOIL_MOISTURE}}", String(latestSoilMoisturePercent, 1));
  server.send(200, "text/html", html);
}

// Kiểm tra xem có đến thời gian gửi báo cáo chưa
bool isTimeToSendDailyReport() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Không thể lấy thời gian");
    return false;
  }
  
  // Nếu đúng giờ gửi báo cáo và chưa gửi báo cáo hôm nay
  if (timeinfo.tm_hour == dailyReportHour && 
      timeinfo.tm_min == dailyReportMinute && 
      !reportSentToday) {
    return true;
  }
  
  // Reset trạng thái gửi báo cáo vào 0:00
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
    reportSentToday = false;
  }
  
  return false;
}

// Gửi báo cáo chi tiết đến Gemini để phân tích
void sendDetailedReportToGemini(JsonObject& summary) {
  Serial.println("Đang gửi dữ liệu chi tiết đến Gemini để phân tích...");
  
  String date = summary["date"].as<String>();
  int readings = summary["readings"];
  float avgTemp = summary["avgTemp"].as<float>();
  float maxTemp = summary["maxTemp"].as<float>();
  float minTemp = summary["minTemp"].as<float>();
  float avgHumidity = summary["avgHumidity"].as<float>();
  float avgSoilMoisture = summary["avgSoilMoisture"].as<float>();
  float maxSoilMoisture = summary["maxSoilMoisture"].as<float>();
  float minSoilMoisture = summary["minSoilMoisture"].as<float>();
  
  // Chuẩn bị dữ liệu chi tiết từ mảng data nhưng giới hạn số lượng điểm dữ liệu
  String dataPoints = "";
  JsonArray dataArray = summary["data"].as<JsonArray>();
  
  // Giới hạn số lượng điểm dữ liệu gửi đến Gemini để tránh timeout
  int dataPointLimit = (dataArray.size() < 8) ? dataArray.size() : 8; // Giới hạn 8 điểm dữ liệu
  int step = dataArray.size() / dataPointLimit;
  if (step < 1) step = 1;
  
  for (int i = 0; i < dataArray.size(); i += step) {
    if (dataPoints.length() > 500) { // Giới hạn kích thước dữ liệu chi tiết
      dataPoints += "- ... và " + String(dataArray.size() - i) + " điểm dữ liệu khác\n";
      break;
    }
    
    JsonObject point = dataArray[i].as<JsonObject>();
    dataPoints += "- Thời gian: " + point["time"].as<String>() + ", ";
    dataPoints += "Nhiệt độ: " + String(point["temperature"].as<float>(), 1) + "°C, ";
    dataPoints += "Độ ẩm không khí: " + String(point["humidity"].as<float>(), 0) + "%, ";
    dataPoints += "Độ ẩm đất: " + String(point["soil_moisture"].as<float>(), 0) + "%\n";
  }
  
  // Tạo prompt hỏi Gemini - làm ngắn gọn để giảm kích thước
  String prompt = "Bạn là chuyên gia nông nghiệp, phân tích dữ liệu cảm biến sau:\n\n";
  prompt += "Ngày: " + date + "\n";
  prompt += "Số đo: " + String(readings) + "\n";
  prompt += "Nhiệt độ: TB=" + String(avgTemp, 1) + "°C, Max=" + String(maxTemp, 1) + "°C, Min=" + String(minTemp, 1) + "°C\n";
  prompt += "Độ ẩm không khí TB: " + String(avgHumidity, 1) + "%\n";
  prompt += "Độ ẩm đất: TB=" + String(avgSoilMoisture, 1) + "%, Max=" + String(maxSoilMoisture, 1) + "%, Min=" + String(minSoilMoisture, 1) + "%\n\n";
  
  prompt += "Dữ liệu mẫu:\n";
  prompt += dataPoints + "\n";
  
  prompt += "Yêu cầu:\n";
  prompt += "1. Phân tích môi trường (nhiệt độ, độ ẩm không khí, độ ẩm đất) trong ngày.\n";
  prompt += "2. Biến động nhiệt độ và độ ẩm trong ngày.\n";
  prompt += "3. Đánh giá mức độ phù hợp cho cây cà chua dựa trên các thông số trên.\n";
  prompt += "4. Đề xuất biện pháp tối ưu điều kiện trồng trọt, đặc biệt là lịch tưới nước dựa trên độ ẩm đất.\n";
  prompt += "5. Dự báo rủi ro sâu bệnh, nấm mốc.\n\n";
  
  prompt += "Trình bày ngắn gọn, chuyên nghiệp, tối đa 250 từ.";
  
  // Hiển thị thông báo đang gửi
  bot.sendMessage(CHAT_ID_1, "🔍 *ĐANG PHÂN TÍCH DỮ LIỆU*\n\nVui lòng đợi trong giây lát...", "Markdown");
  bot.sendMessage(CHAT_ID_2, "🔍 *ĐANG PHÂN TÍCH DỮ LIỆU*\n\nVui lòng đợi trong giây lát...", "Markdown");
  
  // Gửi yêu cầu đến Gemini
  WiFiClientSecure gemini_client;
  gemini_client.setInsecure();
  gemini_client.setTimeout(30); // Tăng timeout lên 30 giây

  HTTPClient https;
  https.setConnectTimeout(30000); // 30 giây timeout kết nối
  https.setTimeout(30000); // 30 giây timeout cho toàn bộ request
  
  String gemini_url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(Gemini_Token);
  if (https.begin(gemini_client, gemini_url)) {
    https.addHeader("Content-Type", "application/json");

    prompt.replace("\"", "\\\"");
    String payload = "{\"contents\": [{\"parts\":[{\"text\":\"" + prompt + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": 1024, \"temperature\": 0.4}}";
    
    Serial.println("Gửi yêu cầu đến Gemini, kích thước payload: " + String(payload.length()) + " bytes");
    int httpCode = https.POST(payload);
    
    Serial.println("Phản hồi HTTP code: " + String(httpCode));
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      String response = https.getString();
      DynamicJsonDocument doc(16384); // Tăng kích thước buffer để xử lý phản hồi lớn
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        String analysis = doc["candidates"][0]["content"]["parts"][0]["text"];
        analysis.trim();
        
        // Thêm tiêu đề và thông tin tổng quan
        String reportMessage = "📊 *BÁO CÁO PHÂN TÍCH* 📊\n\n";
        reportMessage += "📅 *Ngày*: " + date + "\n";
        reportMessage += "🌡️ *Nhiệt độ*: " + String(avgTemp, 1) + "°C (TB), " + String(maxTemp, 1) + "°C (Max), " + String(minTemp, 1) + "°C (Min)\n";
        reportMessage += "💧 *Độ ẩm không khí TB*: " + String(avgHumidity, 1) + "%\n";
        reportMessage += "🌱 *Độ ẩm đất TB*: " + String(avgSoilMoisture, 1) + "%\n\n";
        
        // Thay thế các từ khóa bằng emojis để làm báo cáo sinh động hơn
        analysis.replace("Nhiệt độ", "🌡️ Nhiệt độ");
        analysis.replace("Độ ẩm không khí", "💧 Độ ẩm không khí");
        analysis.replace("Độ ẩm đất", "🌱 Độ ẩm đất");
        analysis.replace("Phân tích", "📊 Phân tích");
        analysis.replace("Khuyến nghị", "💡 Khuyến nghị");
        analysis.replace("Lưu ý", "⚠️ Lưu ý");
        analysis.replace("Kết luận", "✅ Kết luận");
        analysis.replace("Dự báo", "🔮 Dự báo");
        analysis.replace("Rủi ro", "⚠️ Rủi ro");
        
        // Store the analysis results for web access
        lastAnalysisResults = reportMessage + analysis;
        // Set analysis flag to complete
        analysisInProgress = false;
        
        // Gửi phân tích
        // Telegram có giới hạn kích thước tin nhắn (~4000 ký tự)
        const int MAX_MESSAGE_SIZE = 3800;
        
        if (analysis.length() <= MAX_MESSAGE_SIZE) {
          // Gửi nguyên tin nhắn nếu đủ ngắn
          bot.sendMessage(CHAT_ID_1, reportMessage + analysis, "Markdown");
          bot.sendMessage(CHAT_ID_2, reportMessage + analysis, "Markdown");
        } else {
          // Gửi phần đầu (thông tin tổng quan)
          bot.sendMessage(CHAT_ID_1, reportMessage, "Markdown");
          bot.sendMessage(CHAT_ID_2, reportMessage, "Markdown");
          
          // Chia phân tích thành nhiều phần
          int msgCount = (analysis.length() + MAX_MESSAGE_SIZE - 1) / MAX_MESSAGE_SIZE;
          for (int i = 0; i < msgCount; i++) {
            int startPos = i * MAX_MESSAGE_SIZE;
            int endPos = min((i + 1) * MAX_MESSAGE_SIZE, (int)analysis.length());
            String part = analysis.substring(startPos, endPos);
            
            String partHeader = "*PHẦN " + String(i + 1) + "/" + String(msgCount) + "*\n\n";
            bot.sendMessage(CHAT_ID_1, partHeader + part, "Markdown");
            bot.sendMessage(CHAT_ID_2, partHeader + part, "Markdown");
            
            // Đợi một chút giữa các tin nhắn để tránh spam
            delay(500);
          }
        }
      } else {
        Serial.println("Lỗi phân tích JSON từ Gemini: " + String(error.c_str()));
        lastAnalysisResults = "❌ Error: Could not parse Gemini response.";
        analysisInProgress = false;
        bot.sendMessage(CHAT_ID_1, "❌ Lỗi: Không thể phân tích phản hồi từ Gemini.", "Markdown");
        bot.sendMessage(CHAT_ID_2, "❌ Lỗi: Không thể phân tích phản hồi từ Gemini.", "Markdown");
      }
    } else {
      Serial.printf("[HTTPS] POST error: %s\n", https.errorToString(httpCode).c_str());
      lastAnalysisResults = "❌ Error: Could not connect to Gemini API.";
      analysisInProgress = false;
      
      // Phân tích đơn giản nếu Gemini timeout
      String fallbackMessage = "📊 *BÁO CÁO PHÂN TÍCH CƠ BẢN* 📊\n\n";
      fallbackMessage += "📅 *Ngày*: " + date + "\n";
      fallbackMessage += "🌡️ *Nhiệt độ*: " + String(avgTemp, 1) + "°C (TB), " + String(maxTemp, 1) + "°C (Max), " + String(minTemp, 1) + "°C (Min)\n";
      fallbackMessage += "💧 *Độ ẩm không khí TB*: " + String(avgHumidity, 1) + "%\n";
      fallbackMessage += "🌱 *Độ ẩm đất TB*: " + String(avgSoilMoisture, 1) + "%\n\n";
      
      fallbackMessage += "📝 *PHÂN TÍCH CƠ BẢN*:\n\n";
      
      // Phân tích nhiệt độ
      if (avgTemp > 32) {
        fallbackMessage += "🌡️ *Nhiệt độ cao*: Nhiệt độ trung bình (" + String(avgTemp, 1) + "°C) cao hơn mức tối ưu cho cây cà chua (21-29°C). Cần tưới nước thường xuyên và tạo bóng râm.\n\n";
      } else if (avgTemp < 18) {
        fallbackMessage += "🌡️ *Nhiệt độ thấp*: Nhiệt độ trung bình (" + String(avgTemp, 1) + "°C) thấp hơn ngưỡng tối thiểu cho cây cà chua (18°C). Cân nhắc sử dụng màng phủ hoặc nhà kính.\n\n";
      } else if (avgTemp >= 18 && avgTemp <= 29) {
        fallbackMessage += "🌡️ *Nhiệt độ phù hợp*: Nhiệt độ trung bình (" + String(avgTemp, 1) + "°C) nằm trong khoảng tối ưu cho cây cà chua (18-29°C).\n\n";
      } else {
        fallbackMessage += "🌡️ *Nhiệt độ cao vừa phải*: Nhiệt độ trung bình (" + String(avgTemp, 1) + "°C) hơi cao nhưng vẫn có thể chấp nhận được. Tăng cường tưới nước.\n\n";
      }
      
      // Phân tích độ ẩm không khí
      if (avgHumidity > 80) {
        fallbackMessage += "💧 *Độ ẩm không khí cao*: Độ ẩm không khí trung bình (" + String(avgHumidity, 1) + "%) cao hơn mức tối ưu (65-75%). Rủi ro cao về các bệnh nấm và mốc. Cần cải thiện thông gió và giảm tưới nước.\n\n";
      } else if (avgHumidity < 50) {
        fallbackMessage += "💧 *Độ ẩm không khí thấp*: Độ ẩm không khí trung bình (" + String(avgHumidity, 1) + "%) thấp hơn mức tối ưu (65-75%). Cần tăng cường tưới nước và che phủ đất.\n\n";
      } else {
        fallbackMessage += "💧 *Độ ẩm không khí phù hợp*: Độ ẩm không khí trung bình (" + String(avgHumidity, 1) + "%) gần với khoảng tối ưu (65-75%) cho cây cà chua.\n\n";
      }
      
      // Phân tích độ ẩm đất
      if (avgSoilMoisture > 80) {
        fallbackMessage += "🌱 *Độ ẩm đất cao*: Độ ẩm đất trung bình (" + String(avgSoilMoisture, 1) + "%) quá cao. Cần giảm tưới nước để tránh úng và thối rễ.\n\n";
      } else if (avgSoilMoisture < 30) {
        fallbackMessage += "🌱 *Độ ẩm đất thấp*: Độ ẩm đất trung bình (" + String(avgSoilMoisture, 1) + "%) quá thấp. Cần tăng cường tưới nước ngay lập tức.\n\n";
      } else if (avgSoilMoisture >= 30 && avgSoilMoisture <= 60) {
        fallbackMessage += "🌱 *Độ ẩm đất tốt*: Độ ẩm đất trung bình (" + String(avgSoilMoisture, 1) + "%) nằm trong khoảng lý tưởng (30-60%) cho cây cà chua.\n\n";
      } else {
        fallbackMessage += "🌱 *Độ ẩm đất hơi cao*: Độ ẩm đất trung bình (" + String(avgSoilMoisture, 1) + "%) hơi cao nhưng chấp nhận được. Hạn chế tưới nước trong vài ngày tới.\n\n";
      }
      
      // Khuyến nghị
      fallbackMessage += "💡 *KHUYẾN NGHỊ*:\n";
      
      // Khuyến nghị dựa trên cả nhiệt độ, độ ẩm không khí và độ ẩm đất
      if (avgTemp > 30 && avgHumidity > 80) {
        fallbackMessage += "- Cải thiện thông gió để giảm độ ẩm không khí\n";
        fallbackMessage += "- Tưới nước vào buổi sáng sớm\n";
        fallbackMessage += "- Theo dõi các dấu hiệu bệnh nấm\n";
        fallbackMessage += "- Phun thuốc phòng bệnh nếu cần\n";
      } else if (avgTemp > 30 && avgHumidity < 60 && avgSoilMoisture < 40) {
        fallbackMessage += "- Tăng tưới nước, tối ưu vào sáng sớm và chiều tối\n";
        fallbackMessage += "- Che phủ đất để giữ ẩm\n";
        fallbackMessage += "- Tạo bóng râm cho cây trong những giờ nắng gắt\n";
      } else if (avgTemp < 20 && avgSoilMoisture > 70) {
        fallbackMessage += "- Giảm tưới nước khi nhiệt độ thấp\n";
        fallbackMessage += "- Sử dụng màng phủ để giữ nhiệt\n";
        fallbackMessage += "- Tưới nước vào buổi trưa khi ấm nhất\n";
      } else if (avgSoilMoisture < 30) {
        fallbackMessage += "- Tăng gấp đôi lượng nước tưới\n";
        fallbackMessage += "- Che phủ đất để giảm bay hơi\n";
        fallbackMessage += "- Tưới 2 lần/ngày vào sáng sớm và chiều tối\n";
      } else if (avgSoilMoisture > 80) {
        fallbackMessage += "- Ngừng tưới nước trong 2-3 ngày\n";
        fallbackMessage += "- Cải thiện thoát nước xung quanh khu vực trồng\n";
        fallbackMessage += "- Kiểm tra rễ cây để phát hiện dấu hiệu thối rễ\n";
      }
      
      fallbackMessage += "\n⚠️ *Lưu ý*: Đây là phân tích cơ bản do kết nối đến Gemini không thành công. Để có phân tích chi tiết, hãy thử lại sau.";
      
      bot.sendMessage(CHAT_ID_1, fallbackMessage, "Markdown");
      bot.sendMessage(CHAT_ID_2, fallbackMessage, "Markdown");
    }
    https.end();
  } else {
    Serial.println("[HTTPS] Không thể kết nối đến Gemini");
    lastAnalysisResults = "❌ Error: Could not connect to Gemini API.";
    analysisInProgress = false;
    bot.sendMessage(CHAT_ID_1, "❌ Không thể kết nối đến máy chủ Gemini", "Markdown");
    bot.sendMessage(CHAT_ID_2, "❌ Không thể kết nối đến máy chủ Gemini", "Markdown");
  }
}

// Lưu dữ liệu báo cáo hàng ngày
DynamicJsonDocument lastDailyReport(8192);
void saveDailyReport(JsonObject summary) {
  lastDailyReport.clear();
  lastDailyReport.set(summary);
}

// Gửi báo cáo hàng ngày từ Google Sheets
void sendDailyReport() {
  Serial.println("Đang tạo báo cáo hàng ngày...");
  
  reportInProgress = true;
  reportStartTime = millis();
  
  // Lấy thời gian hiện tại
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Không thể lấy thời gian");
    reportInProgress = false;
    lastReportResults = "❌ Error: Could not get current time.";
    return;
  }
  
  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  
  // URL để lấy báo cáo từ Google Script
  String urlFinal = "https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?";
  urlFinal += "action=getDailyReport";
  urlFinal += "&date=" + String(dateStr);
  
  Serial.print("Gửi yêu cầu lấy báo cáo: ");
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
    
    // Chỉ phân tích JSON nếu phản hồi bắt đầu bằng {
    if (payload.startsWith("{")) {
      // Phân tích phản hồi JSON
      DynamicJsonDocument doc(8192); // Tăng kích thước buffer để xử lý nhiều dữ liệu
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        if (doc.containsKey("summary")) {
          // Nếu có dữ liệu tóm tắt
          float avgTemp = doc["summary"]["avgTemp"];
          float maxTemp = doc["summary"]["maxTemp"];
          float minTemp = doc["summary"]["minTemp"];
          float avgHumidity = doc["summary"]["avgHumidity"];
          float avgSoilMoisture = doc["summary"]["avgSoilMoisture"];
          float maxSoilMoisture = doc["summary"]["maxSoilMoisture"];
          float minSoilMoisture = doc["summary"]["minSoilMoisture"];
          int readings = doc["summary"]["readings"];
          
          // Tạo tin nhắn báo cáo
          String message = "📊 *BÁO CÁO HÀNG NGÀY* 📊\n\n";
          message += "📅 *Ngày*: " + String(dateStr) + "\n";
          message += "🔢 *Số đo*: " + String(readings) + " lần\n\n";
          message += "🌡️ *Nhiệt độ*:\n";
          message += "  • Trung bình: " + String(avgTemp, 1) + " °C\n";
          message += "  • Cao nhất: " + String(maxTemp, 1) + " °C\n";
          message += "  • Thấp nhất: " + String(minTemp, 1) + " °C\n\n";
          message += "💧 *Độ ẩm không khí trung bình*: " + String(avgHumidity, 1) + " %\n\n";
          message += "🌱 *Độ ẩm đất*:\n";
          message += "  • Trung bình: " + String(avgSoilMoisture, 1) + " %\n";
          message += "  • Cao nhất: " + String(maxSoilMoisture, 1) + " %\n";
          message += "  • Thấp nhất: " + String(minSoilMoisture, 1) + " %\n\n";
          
          // Thêm phân tích đơn giản
          message += "📝 *PHÂN TÍCH*:\n";
          if (avgTemp > 30) {
            message += "⚠️ Nhiệt độ trung bình cao, cần chú ý tưới nước cho cây\n";
          } else if (avgTemp < 18) {
            message += "⚠️ Nhiệt độ trung bình thấp, cần chú ý giữ ấm cho cây\n";
          } else {
            message += "✅ Nhiệt độ trung bình phù hợp cho cây phát triển\n";
          }
          
          if (avgHumidity > 80) {
            message += "⚠️ Độ ẩm không khí cao, có thể dẫn đến nấm bệnh\n";
          } else if (avgHumidity < 40) {
            message += "⚠️ Độ ẩm không khí thấp, cần tăng cường tưới nước\n";
          } else {
            message += "✅ Độ ẩm không khí phù hợp cho cây phát triển\n";
          }
          
          if (avgSoilMoisture > 80) {
            message += "⚠️ Độ ẩm đất cao, cần giảm tưới nước để tránh úng\n";
          } else if (avgSoilMoisture < 30) {
            message += "⚠️ Độ ẩm đất thấp, cần tăng cường tưới nước\n";
          } else {
            message += "✅ Độ ẩm đất phù hợp cho rễ cây phát triển\n";
          }
          
          message += "\n📎 Đường dẫn đến báo cáo đầy đủ:\nhttps://docs.google.com/spreadsheets/d/1TL3eZKGvPJPkzvwfWgkRNlIFvacSC1WcySUlwyRMPnA/edit";
          message += "\n\n💡 Để xem phân tích chi tiết, gửi lệnh /analysis";
          
          // Store the report for web access
          lastReportResults = message;
          reportInProgress = false;
          
          // Gửi báo cáo qua Telegram
          bot.sendMessage(CHAT_ID_1, message, "Markdown");
          bot.sendMessage(CHAT_ID_2, message, "Markdown");
          
          // Đánh dấu đã gửi báo cáo hôm nay
          reportSentToday = true;
          Serial.println("Đã gửi báo cáo hàng ngày thành công");
          
          // Lưu dữ liệu báo cáo để có thể phân tích chi tiết sau
          saveDailyReport(doc["summary"]);
          
        } else if (doc.containsKey("error")) {
          // Xử lý lỗi từ Google Script
          String errorMsg = doc["error"].as<String>();
          String message = "❌ *KHÔNG CÓ DỮ LIỆU BÁO CÁO* ❌\n\n";
          message += errorMsg;
          
          // Store the error message for web access
          lastReportResults = message;
          reportInProgress = false;
          
          bot.sendMessage(CHAT_ID_1, message, "Markdown");
          bot.sendMessage(CHAT_ID_2, message, "Markdown");
          Serial.println("Lỗi từ Google Script: " + errorMsg);
        } else {
          // Không có dữ liệu
          String message = "❌ *KHÔNG CÓ DỮ LIỆU BÁO CÁO* ❌\n\n";
          message += "Không có dữ liệu cho ngày " + String(dateStr);
          
          // Store the error message for web access
          lastReportResults = message;
          reportInProgress = false;
          
          bot.sendMessage(CHAT_ID_1, message, "Markdown");
          bot.sendMessage(CHAT_ID_2, message, "Markdown");
          Serial.println("Không có dữ liệu cho báo cáo hàng ngày");
        }
      } else {
        Serial.println("Lỗi phân tích JSON: " + String(error.c_str()));
        Serial.println("Dữ liệu nhận được: " + payload);
        
        // Store the error message for web access
        lastReportResults = "❌ Lỗi phân tích dữ liệu báo cáo!";
        reportInProgress = false;
        
        bot.sendMessage(CHAT_ID_1, "❌ Lỗi phân tích dữ liệu báo cáo!", "");
        bot.sendMessage(CHAT_ID_2, "❌ Lỗi phân tích dữ liệu báo cáo!", "");
      }
    } else {
      Serial.println("Phản hồi không phải là JSON hợp lệ: " + payload);
      
      // Store the error message for web access
      lastReportResults = "❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!";
      reportInProgress = false;
      
      bot.sendMessage(CHAT_ID_1, "❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!", "");
      bot.sendMessage(CHAT_ID_2, "❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!", "");
    }
  } else {
    Serial.println("Lỗi kết nối HTTP, code: " + String(httpCode));
    
    // Store the error message for web access
    lastReportResults = "❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode);
    reportInProgress = false;
    
    bot.sendMessage(CHAT_ID_1, "❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode), "");
    bot.sendMessage(CHAT_ID_2, "❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode), "");
  }
  
  http.end();
}

// Yêu cầu phân tích chi tiết từ Gemini về báo cáo đã lưu
void requestDetailedAnalysis() {
  if (lastDailyReport.size() > 0) {
    // Set the analysis in progress flag
    analysisInProgress = true;
    analysisStartTime = millis();
    lastAnalysisResults = ""; // Clear previous results
    
    JsonObject summary = lastDailyReport.as<JsonObject>();
    sendDetailedReportToGemini(summary);
  } else {
    lastAnalysisResults = "❌ No recent report data available for analysis.";
    bot.sendMessage(CHAT_ID_1, "❌ Không có dữ liệu báo cáo gần đây để phân tích!", "Markdown");
    bot.sendMessage(CHAT_ID_2, "❌ Không có dữ liệu báo cáo gần đây để phân tích!", "Markdown");
  }
}

// Gửi dữ liệu lên Google Sheets
void sendDataToGoogleSheets() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Đọc giá trị độ ẩm đất
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100); // Giới hạn giá trị từ 0-100%
  
  // Kiểm tra dữ liệu hợp lệ
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
  
  // Lấy thời gian hiện tại
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Không thể lấy thời gian");
    return;
  }
  
  char dateStr[11];
  char timeStr[9];
  strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  
  // Chuẩn bị dữ liệu để gửi
  String urlFinal = "https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?";
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
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Theo chuyển hướng HTTP 302
  http.setTimeout(15000); // Tăng timeout lên 15 giây
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
    
    bot.sendMessage(CHAT_ID_1, message, "Markdown");
    bot.sendMessage(CHAT_ID_2, message, "Markdown");
  } else {
    Serial.println("Lỗi kết nối HTTP, code: " + String(httpCode));
    Serial.println("Chi tiết lỗi: " + http.errorToString(httpCode));
    bot.sendMessage(CHAT_ID_1, "❌ Lỗi cập nhật dữ liệu lên Google Sheets! Code: " + String(httpCode), "");
    bot.sendMessage(CHAT_ID_2, "❌ Lỗi cập nhật dữ liệu lên Google Sheets! Code: " + String(httpCode), "");
  }
  
  http.end();
}

// Function to handle sending data to Google Sheets from web interface
void handleUpdateToSheets() {
  Serial.println("Sending data to Google Sheets via web interface");
  
  // Call the function to send data to Google Sheets
  sendDataToGoogleSheets();
  
  // Send success response
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Data sent to Google Sheets\"}");
}

// Function to extract values from JSON text
String extractValue(String jsonText, String key) {
  int keyPos = jsonText.indexOf("\"" + key + "\"");
  if (keyPos == -1) {
    return "Không xác định";
  }
  
  int valueStart = jsonText.indexOf("\"", keyPos + key.length() + 3) + 1;
  if (valueStart == 0) {
    return "Không xác định";
  }
  
  int valueEnd = jsonText.indexOf("\"", valueStart);
  if (valueEnd == -1) {
    return "Không xác định";
  }
  
  return jsonText.substring(valueStart, valueEnd);
}

void getWeatherAndAskGemini() {
  WiFiClientSecure client;
  client.setInsecure(); // Bỏ qua kiểm tra chứng chỉ SSL

  // Lấy dự báo hiện tại và 7 ngày
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude) +
               "&longitude=" + String(longitude) +
               "&current=temperature_2m,relative_humidity_2m,pressure_msl,wind_speed_10m" +
               "&daily=temperature_2m_max,temperature_2m_min,relative_humidity_2m_max,pressure_msl_max,wind_speed_10m_max,precipitation_sum" +
               "&forecast_days=7&timezone=" + String(timezone);

  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();

  String weather_summary = "";

  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(9216);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Xử lý dữ liệu hiện tại
      JsonObject current = doc["current"];
      weather_summary += "=== Current Weather ===\n";
      weather_summary += "Temperature: " + String(current["temperature_2m"].as<float>()) + " °C\n";
      weather_summary += "Humidity: " + String(current["relative_humidity_2m"].as<int>()) + "%\n";
      weather_summary += "Pressure: " + String(current["pressure_msl"].as<float>()) + " hPa\n";
      weather_summary += "Wind Speed: " + String(current["wind_speed_10m"].as<float>()) + " m/s\n\n";

      // Xử lý dự báo 7 ngày
      JsonArray temp_max = doc["daily"]["temperature_2m_max"];
      JsonArray temp_min = doc["daily"]["temperature_2m_min"];
      JsonArray humidity = doc["daily"]["relative_humidity_2m_max"];
      JsonArray pressure = doc["daily"]["pressure_msl_max"];
      JsonArray wind = doc["daily"]["wind_speed_10m_max"];
      JsonArray rain = doc["daily"]["precipitation_sum"];
      JsonArray date = doc["daily"]["time"];

      weather_summary += "=== 7-Day Forecast ===\n";
      for (int i = 0; i < 7; i++) {
        String line = "Day " + String(i+1) + " (" + String(date[i].as<const char*>()) + "): ";
        line += "Max/Min Temp: " + String(temp_max[i].as<float>()) + "/" + String(temp_min[i].as<float>()) + " °C, ";
        line += "Humidity: " + String(humidity[i].as<int>()) + "%, ";
        line += "Pressure: " + String(pressure[i].as<float>()) + " hPa, ";
        line += "Wind: " + String(wind[i].as<float>()) + " m/s, ";
        line += "Rain: " + String(rain[i].as<float>()) + " mm";
        weather_summary += line + "\n";
      }
      Serial.println(weather_summary);
      
    } else {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
      return;
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
    return;
  }
  http.end();

  // Tạo prompt hỏi Gemini - tùy thuộc vào trạng thái bệnh của cây
  String prompt;
  
  // Kiểm tra trạng thái bệnh
  bool hasDiseaseCondition = (predictedDisease != "Không có" && 
                            predictedDisease != "Healthy" && 
                            predictedDisease != "healthy");
  
  if (hasDiseaseCondition) {
    // Nếu cây đang bị bệnh, sử dụng prompt chuyên cho bệnh
    prompt = "Với vai trò là chuyên gia nông nghiệp về bệnh cây trồng, hãy phân tích dữ liệu thời tiết sau đây:\n\n";
    prompt += "===== THÔNG TIN THỜI TIẾT =====\n";
    prompt += weather_summary;
    prompt += "\n\n===== THÔNG TIN BỆNH CÂY =====\n";
    prompt += "Loại bệnh đã phát hiện: " + predictedDisease + "\n\n";
    prompt += "===== YÊU CẦU PHÂN TÍCH =====\n";
    prompt += "1. Dựa vào các biến số thời tiết (nhiệt độ, độ ẩm, lượng mưa, gió) và tình trạng bệnh của cây, xác định thời điểm tối ưu để bón phân cho cây trồng trong 7 ngày tới, lưu ý đến việc cây đang bị bệnh " + predictedDisease + ".\n";
    prompt += "2. Xác định CHÍNH XÁC MỘT GIỜ cụ thể (VD: 17:00 hoặc 6:30) lý tưởng để tưới cây cà chua trong ngày hôm nay, có tính đến các yếu tố thời tiết, sinh lý cây trồng, và đặc biệt là bệnh " + predictedDisease + " của cây.\n";
    prompt += "3. Giải thích lý do cho những đề xuất trên (dựa trên các nguyên tắc nông học và kiến thức về bệnh cây).\n";
    prompt += "4. Đề xuất cách điều trị bệnh kết hợp với lịch tưới nước và bón phân.\n\n";
  } else {
    // Nếu cây khỏe mạnh, sử dụng prompt cũ
    prompt = "Với vai trò là trợ lý nông nghiệp thông minh, hãy phân tích dữ liệu thời tiết sau đây:\n\n";
    prompt += "===== THÔNG TIN THỜI TIẾT =====\n";
    prompt += weather_summary;
    prompt += "\n\n===== YÊU CẦU PHÂN TÍCH =====\n";
    prompt += "1. Dựa vào các biến số thời tiết (nhiệt độ, độ ẩm, lượng mưa, gió), xác định thời điểm tối ưu để bón phân cho cây trồng trong 7 ngày tới.\n";
    prompt += "2. Xác định CHÍNH XÁC MỘT GIỜ cụ thể (VD: 17:00 hoặc 6:30) lý tưởng để tưới cây cà chua trong ngày hôm nay, có tính đến các yếu tố thời tiết và sinh lý cây trồng.\n";
    prompt += "3. Giải thích lý do cho những đề xuất trên (dựa trên các nguyên tắc nông học).\n\n";
  }
  
  prompt += "===== YÊU CẦU ĐỊNH DẠNG PHẢN HỒI =====\n";
  prompt += "Trả lời dưới dạng JSON theo cấu trúc sau (không có chữ ở ngoài):\n";
  prompt += "{\n";
  prompt += "  \"tomato_watering_time\": \"giờ cụ thể để tưới cây cà chua hôm nay, PHẢI là một thời điểm cụ thể theo định dạng HH:MM (ví dụ: 17:00, 6:00, 18:30)\",\n";
  prompt += "  \"best_fertilization_day\": \"ngày tốt nhất để bón phân trong 7 ngày tới, định dạng ngày/tháng\",\n";
  prompt += "  \"reason\": \"lý do chi tiết cho các đề xuất trên, bao gồm các yếu tố thời tiết và nông học";
  
  // Thêm trường treatment nếu có bệnh
  if (hasDiseaseCondition) {
    prompt += "\",\n  \"treatment\": \"đề xuất điều trị cụ thể cho bệnh " + predictedDisease + " kết hợp với lịch tưới nước và bón phân";
  }
  
  prompt += "\"\n}\n\n";
  prompt += "LƯU Ý: Đây là dữ liệu cho hệ thống tự động tưới cây thông minh dùng ESP32, vì vậy trả lời phải CHÍNH XÁC theo định dạng JSON đã yêu cầu, không thêm bất kỳ ký tự nào khác.";

  WiFiClientSecure gemini_client;
  gemini_client.setInsecure();

  HTTPClient https;
  String gemini_url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(Gemini_Token);
  if (https.begin(gemini_client, gemini_url)) {
    https.addHeader("Content-Type", "application/json");

    prompt.replace("\"", "\\\"");
    String payload = "{\"contents\": [{\"parts\":[{\"text\":\"" + prompt + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": " + String(Gemini_Max_Tokens) + "}}";
    
    int httpCode = https.POST(payload);
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      String response = https.getString();
      DynamicJsonDocument doc(4096);
      deserializeJson(doc, response);
      String Answer = doc["candidates"][0]["content"]["parts"][0]["text"];
      Answer.trim();

      Serial.println("\n===== Gemini Suggestion =====");
      Serial.println(Answer);

      // Trích xuất thông tin quan trọng
      String wateringTime = extractValue(Answer, "tomato_watering_time");
      String fertilizingDay = extractValue(Answer, "best_fertilization_day");
      String reason = extractValue(Answer, "reason");
      
      // Lưu thời gian tưới nước được đề xuất
      scheduledWateringTime = wateringTime;
      wateringScheduleActive = true;  // Kích hoạt lịch tưới nước
      alreadyWateredToday = false;    // Reset trạng thái tưới nước
      
      // Log thời gian tưới nước được đặt lịch
      Serial.print("Đã đặt lịch tưới nước lúc: ");
      Serial.println(scheduledWateringTime);
      
      // Tạo thông báo ngắn gọn, tập trung vào thông tin quan trọng
      String formattedMessage;
      
      if (hasDiseaseCondition) {
        String treatment = extractValue(Answer, "treatment");
        formattedMessage = "🌱 *DỰ BÁO THỜI TIẾT & KHUYẾN NGHỊ ĐIỀU TRỊ* 🌱\n\n";
        formattedMessage += "🔬 *Bệnh phát hiện*: " + predictedDisease + "\n\n";
        formattedMessage += "⏰ *Giờ tưới cây tối ưu hôm nay*: " + wateringTime + "\n\n";
        formattedMessage += "📅 *Ngày bón phân tốt nhất*: " + fertilizingDay + "\n\n";
        formattedMessage += "💡 *Lý do*: " + reason + "\n\n";
        formattedMessage += "💊 *Khuyến nghị điều trị*: " + treatment + "\n";
      } else {
        formattedMessage = "🌱 *DỰ BÁO THỜI TIẾT & KHUYẾN NGHỊ* 🌱\n\n";
        formattedMessage += "⏰ *Giờ tưới cây tối ưu hôm nay*: " + wateringTime + "\n\n";
        formattedMessage += "📅 *Ngày bón phân tốt nhất*: " + fertilizingDay + "\n\n";
        formattedMessage += "💡 *Lý do*: " + reason + "\n";
      }

      bot.sendMessage(CHAT_ID_1, formattedMessage, "Markdown");
      bot.sendMessage(CHAT_ID_2, formattedMessage, "Markdown");
    } else {
      Serial.printf("[HTTPS] POST error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("[HTTPS] Connection failed");
  }
}

// Hàm kiểm tra kết nối đến Google Script
void testGoogleScriptConnection() {
  Serial.println("Kiểm tra kết nối đến Google Script...");
  
  String urlFinal = "https://script.google.com/macros/s/"+GOOGLE_SCRIPT_ID+"/exec?";
  urlFinal += "action=test";
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, urlFinal);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Theo chuyển hướng HTTP 302
  http.setTimeout(15000); // Tăng timeout lên 15 giây
  
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
        
        bot.sendMessage(CHAT_ID_1, message, "Markdown");
        bot.sendMessage(CHAT_ID_2, message, "Markdown");
      } else {
        String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
        message += "❌ Kết nối đến Google Script không thành công!\n";
        message += "📊 HTTP code: " + String(httpCode) + "\n";
        message += "❓ Lỗi: Không thể phân tích phản hồi JSON\n";
        message += "📜 Phản hồi: " + payload;
        
        bot.sendMessage(CHAT_ID_1, message, "Markdown");
        bot.sendMessage(CHAT_ID_2, message, "Markdown");
      }
    } else {
      String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
      message += "⚠️ Kết nối đến Google Script không bình thường!\n";
      message += "📊 HTTP code: " + String(httpCode) + "\n";
      message += "❓ Lỗi: Phản hồi không phải là JSON\n";
      message += "📜 Phản hồi: " + payload;
      
      bot.sendMessage(CHAT_ID_1, message, "Markdown");
      bot.sendMessage(CHAT_ID_2, message, "Markdown");
    }
  } else {
    String message = "📡 *KIỂM TRA KẾT NỐI* 📡\n\n";
    message += "❌ Không thể kết nối đến Google Script!\n";
    message += "📊 HTTP code: " + String(httpCode) + "\n";
    message += "❓ Lỗi: " + http.errorToString(httpCode);
    
    bot.sendMessage(CHAT_ID_1, message, "Markdown");
    bot.sendMessage(CHAT_ID_2, message, "Markdown");
  }
  
  http.end();
}

// Hàm gửi thông báo lỗi
void sendErrorMessage(String errorType, String errorDetail, String diseaseName) {
  String errorMsg = "❌ *" + errorType + "* ❌\n\n";
  errorMsg += errorDetail + "\n\n";
  
  // Thêm khuyến nghị cơ bản nếu đang có bệnh
  if (diseaseName != "Không có" && diseaseName != "Healthy" && diseaseName != "healthy") {
    errorMsg += "Đối với bệnh " + diseaseName + ", đề xuất chung:\n";
    errorMsg += "• Tách cây bị bệnh ra khỏi khu vực\n";
    errorMsg += "• Loại bỏ bộ phận cây bị nhiễm\n";
    errorMsg += "• Tưới nước vào buổi sáng sớm (5-7h)\n";
    errorMsg += "• Tránh bón phân đạm quá nhiều\n";
    errorMsg += "• Tham khảo ý kiến chuyên gia nông nghiệp\n";
  }
  
  bot.sendMessage(CHAT_ID_1, errorMsg, "Markdown");
  bot.sendMessage(CHAT_ID_2, errorMsg, "Markdown");
}

void getTreatmentFromGemini(String diseaseName) {
  WiFiClientSecure gemini_client;
  gemini_client.setInsecure();
  
  // Đọc dữ liệu cảm biến hiện tại
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Lấy thời gian hiện tại
  struct tm timeinfo;
  char timeStr[30] = "Không xác định";
  char dateStr[30] = "Không xác định";
  if (getLocalTime(&timeinfo)) {
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
  }
  
  // Tạo prompt cho Gemini - điều chỉnh theo bệnh phát hiện được
  String prompt;
  
  if (diseaseName == "Không có" || diseaseName == "Healthy" || diseaseName == "healthy") {
    // Nếu không có bệnh, tập trung vào thời tiết và lịch chăm sóc
    prompt = "Bạn là chuyên gia nông nghiệp cho cây cà chua. Hiện tại cây không phát hiện bệnh. ";
    prompt += "Điều kiện môi trường hiện tại: nhiệt độ " + String(temperature, 1) + "°C, độ ẩm " + String(humidity, 1) + "%, ";
    prompt += "thời gian hiện tại là " + String(timeStr) + " ngày " + String(dateStr) + ". ";
    prompt += "Hãy đưa ra lịch trình chăm sóc tối ưu, bao gồm:\n";
    prompt += "1. Thời gian tưới nước lý tưởng trong ngày (giờ cụ thể)\n";
    prompt += "2. Khuyến nghị về tần suất và lượng nước tưới\n";
    prompt += "3. Đề xuất lịch bón phân phù hợp (thời gian và loại phân)\n";
    prompt += "4. Các biện pháp phòng bệnh phù hợp với điều kiện hiện tại\n";
  } else {
    // Nếu có bệnh, tập trung vào điều trị và điều chỉnh lịch chăm sóc
    prompt = "Bạn là chuyên gia nông nghiệp về bệnh cây trồng. Tôi phát hiện cây cà chua có bệnh: \"" + diseaseName + "\". ";
    prompt += "Điều kiện môi trường hiện tại: nhiệt độ " + String(temperature, 1) + "°C, độ ẩm " + String(humidity, 1) + "%, ";
    prompt += "thời gian hiện tại là " + String(timeStr) + " ngày " + String(dateStr) + ". ";
    prompt += "Hãy đưa ra phân tích và đề xuất chi tiết, bao gồm:\n";
    prompt += "1. Mô tả ngắn về bệnh và mức độ nguy hiểm\n";
    prompt += "2. Biện pháp điều trị cụ thể, ưu tiên biện pháp hữu cơ\n";
    prompt += "3. THỜI GIAN TƯỚI NƯỚC PHÙ HỢP (giờ cụ thể, ví dụ: 17:00) để không làm trầm trọng thêm bệnh\n";
    prompt += "4. THỜI GIAN BÓN PHÂN PHÙ HỢP và loại phân nên dùng hoặc tránh khi cây bị bệnh này\n";
    prompt += "5. Biện pháp phòng ngừa lâu dài\n";
  }
  
  prompt += "\nHãy định dạng câu trả lời dễ đọc, ngắn gọn trong khoảng 250 từ và sử dụng emoji phù hợp.";
  
  // Gửi yêu cầu đến Gemini
  HTTPClient https;
  
  // Logging để debug
  Serial.println("Gửi yêu cầu phân tích đến Gemini...");
  
  if (https.begin(gemini_client, "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(Gemini_Token))) {
    https.addHeader("Content-Type", "application/json");
    
    prompt.replace("\"", "\\\"");
    String payload = "{\"contents\": [{\"parts\":[{\"text\":\"" + prompt + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": 1024, \"temperature\": 0.4}}";
    
    int httpCode = https.POST(payload);
    
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      String response = https.getString();
      DynamicJsonDocument doc(10240);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        String analysis = doc["candidates"][0]["content"]["parts"][0]["text"];
        analysis.trim();
        
        // Thêm định dạng và emoji cho phần phân tích
        analysis.replace("Mô tả", "📝 *Mô tả*");
        analysis.replace("Triệu chứng", "🔍 *Triệu chứng*");
        analysis.replace("Tác nhân", "🦠 *Tác nhân*");
        analysis.replace("Điều trị", "💉 *Điều trị*");
        analysis.replace("Phòng ngừa", "🛡️ *Phòng ngừa*");
        analysis.replace("Khuyến nghị", "💡 *Khuyến nghị*");
        analysis.replace("Thời gian tưới nước", "⏰ *Thời gian tưới nước*");
        analysis.replace("Thời gian bón phân", "🌱 *Thời gian bón phân*");
        analysis.replace("Lịch trình", "📅 *Lịch trình*");
        analysis.replace("Lưu ý", "⚠️ *Lưu ý*");
        
        // Tạo thông báo với tiêu đề phù hợp dựa trên tình trạng bệnh
        String messageTitle, messageIcon;
        if (diseaseName == "Không có" || diseaseName == "Healthy" || diseaseName == "healthy") {
          messageTitle = "LỊCH CHĂM SÓC TỐI ƯU";
          messageIcon = "🌿";
        } else {
          messageTitle = "PHÂN TÍCH BỆNH & LỊCH CHĂM SÓC";
          messageIcon = "🔬";
        }
        
        // Tạo thông báo đầy đủ
        String detailedMessage = messageIcon + " *" + messageTitle + "* " + messageIcon + "\n\n";
        
        // Thêm thông tin về bệnh nếu có
        if (diseaseName != "Không có" && diseaseName != "Healthy" && diseaseName != "healthy") {
          detailedMessage += "🌱 *Loại bệnh*: " + diseaseName + "\n";
        }
        
        // Thêm thông tin môi trường
        detailedMessage += "🌡️ *Nhiệt độ*: " + String(temperature, 1) + "°C\n";
        detailedMessage += "💧 *Độ ẩm*: " + String(humidity, 1) + "%\n";
        detailedMessage += "🕒 *Thời gian*: " + String(timeStr) + " - " + String(dateStr) + "\n\n";
        
        // Nội dung phân tích
        detailedMessage += analysis;
        
        // Gửi phân tích chi tiết qua Telegram
        bot.sendMessage(CHAT_ID_1, detailedMessage, "Markdown");
        bot.sendMessage(CHAT_ID_2, detailedMessage, "Markdown");
      } else {
        // Phản hồi lỗi nếu không thể phân tích JSON
        sendErrorMessage("LỖI PHÂN TÍCH", "Không thể phân tích phản hồi JSON từ Gemini.", diseaseName);
      }
    } else {
      // Phản hồi lỗi kết nối HTTP
      sendErrorMessage("LỖI KẾT NỐI", "Không thể kết nối đến dịch vụ Gemini (HTTP code: " + String(httpCode) + ").", diseaseName);
    }
    https.end();
  } else {
    // Phản hồi lỗi không thể bắt đầu kết nối
    sendErrorMessage("LỖI KẾT NỐI", "Không thể thiết lập kết nối đến dịch vụ Gemini.", diseaseName);
  }
}


// Add these new handler functions:
// Function to handle daily report request from the web interface
void handleDailyReport() {
  Serial.println("Daily report requested from web interface");
  
  // If we already have report results, use them
  if (lastReportResults.length() > 0) {
    server.send(200, "text/plain", lastReportResults);
    return;
  }
  
  // If no report available, create a new one
  // Create a buffer to send initial response
  String reportBuffer = "";
  
  // First get the local time
  struct tm timeinfo;
  char dateStr[11];
  if (getLocalTime(&timeinfo)) {
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    reportBuffer = "📊 DAILY REPORT for " + String(dateStr) + " 📊\n\n";
  } else {
    reportBuffer = "📊 DAILY REPORT 📊\n\n";
  }
  
  // Use lastDailyReport if available to provide content for web interface
  if (lastDailyReport.size() > 0) {
    JsonObject summary = lastDailyReport.as<JsonObject>();
    
    float avgTemp = summary["avgTemp"].as<float>();
    float maxTemp = summary["maxTemp"].as<float>();
    float minTemp = summary["minTemp"].as<float>();
    float avgHumidity = summary["avgHumidity"].as<float>();
    float avgSoilMoisture = summary["avgSoilMoisture"].as<float>();
    float maxSoilMoisture = summary["maxSoilMoisture"].as<float>();
    float minSoilMoisture = summary["minSoilMoisture"].as<float>();
    int readings = summary["readings"];
    
    reportBuffer += "Number of readings: " + String(readings) + "\n\n";
    reportBuffer += "TEMPERATURE:\n";
    reportBuffer += "  • Average: " + String(avgTemp, 1) + " °C\n";
    reportBuffer += "  • Maximum: " + String(maxTemp, 1) + " °C\n";
    reportBuffer += "  • Minimum: " + String(minTemp, 1) + " °C\n\n";
    reportBuffer += "AVERAGE AIR HUMIDITY: " + String(avgHumidity, 1) + " %\n\n";
    reportBuffer += "SOIL MOISTURE:\n";
    reportBuffer += "  • Average: " + String(avgSoilMoisture, 1) + " %\n";
    reportBuffer += "  • Maximum: " + String(maxSoilMoisture, 1) + " %\n";
    reportBuffer += "  • Minimum: " + String(minSoilMoisture, 1) + " %\n\n";
    
    reportBuffer += "ANALYSIS:\n";
    if (avgTemp > 30) {
      reportBuffer += "⚠️ Average temperature is high, pay attention to watering\n";
    } else if (avgTemp < 18) {
      reportBuffer += "⚠️ Average temperature is low, keep plants warm\n";
    } else {
      reportBuffer += "✅ Average temperature is suitable for plant growth\n";
    }
    
    if (avgHumidity > 80) {
      reportBuffer += "⚠️ Air humidity is high, watch for fungal diseases\n";
    } else if (avgHumidity < 40) {
      reportBuffer += "⚠️ Air humidity is low, increase watering\n";
    } else {
      reportBuffer += "✅ Air humidity is suitable for plant growth\n";
    }
    
    if (avgSoilMoisture > 80) {
      reportBuffer += "⚠️ Soil moisture is high, reduce watering\n";
    } else if (avgSoilMoisture < 30) {
      reportBuffer += "⚠️ Soil moisture is low, increase watering\n";
    } else {
      reportBuffer += "✅ Soil moisture is suitable for root development\n";
    }
  } else {
    reportBuffer += "No recent data available. Generating new report...\n";
  }
  
  // Call the existing function to generate and send a new report to Telegram
  // This will also update lastReportResults
  sendDailyReport();
  
  reportBuffer += "\nA full report has been sent to Telegram.";
  
  // Send response to client
  server.send(200, "text/plain", reportBuffer);
  
  Serial.println("Daily report sent to web client and Telegram");
}

// Function to handle detailed analysis request from the web interface
void handleDetailedAnalysis() {
  Serial.println("Detailed analysis requested from web interface");
  
  String analysisMsg = "🔍 DETAILED ANALYSIS REQUEST 🔍\n\n";
  
  // Check if we have data to analyze
  if (lastDailyReport.size() > 0) {
    analysisMsg += "Requesting detailed analysis from Gemini AI...\n\n";
    analysisMsg += "This will analyze your plant's environmental conditions and provide recommendations for optimal growth.\n\n";
    analysisMsg += "The complete analysis will be sent to your Telegram account.\n\n";
    analysisMsg += "Note: Analysis can take up to 30 seconds to complete.";
    
    // Call the existing analysis function
    requestDetailedAnalysis();
  } else {
    analysisMsg += "No recent data available for analysis.\n\n";
    analysisMsg += "Please first generate a daily report to collect necessary data.";
  }
  
  // Send a response to the client
  server.send(200, "text/plain", analysisMsg);
  
  Serial.println("Detailed analysis started - results will be sent to Telegram");
}

// Add this new handler function
void handleAnalysisResults() {
  Serial.println("Analysis results requested from web interface");
  
  // Check if results are available
  if (lastAnalysisResults.length() > 0) {
    // Results are available, send them
    server.send(200, "text/plain", lastAnalysisResults);
    return;
  } 
  
  // No results yet, check if analysis is in progress
  if (analysisInProgress) {
    // Check if it's been too long (timeout after 45 seconds)
    if (millis() - analysisStartTime > 45000) {
      // Analysis is taking too long, consider it failed
      lastAnalysisResults = "❌ Analysis timed out after 45 seconds. Please try again.";
      analysisInProgress = false;
      server.send(200, "text/plain", lastAnalysisResults);
    } else {
      // Still processing
      server.send(200, "text/plain", "No analysis results available yet. Analysis is still in progress.");
    }
  } else {
    // No analysis was started or it failed
    server.send(200, "text/plain", "No analysis results available yet. Please request an analysis first.");
  }
}

// Add this new endpoint to get the report results
void handleReportResults() {
  Serial.println("Report results requested from web interface");
  
  // Check if results are available
  if (lastReportResults.length() > 0) {
    // Results are available, send them
    server.send(200, "text/plain", lastReportResults);
    return;
  } 
  
  // No results yet, check if report is in progress
  if (reportInProgress) {
    // Check if it's been too long (timeout after 30 seconds)
    if (millis() - reportStartTime > 30000) {
      // Report generation is taking too long, consider it failed
      lastReportResults = "❌ Report generation timed out after 30 seconds. Please try again.";
      reportInProgress = false;
      server.send(200, "text/plain", lastReportResults);
    } else {
      // Still processing
      server.send(200, "text/plain", "No report results available yet. Report is still being generated.");
    }
  } else {
    // No report was started or it failed
    server.send(200, "text/plain", "No report results available yet. Please request a report first.");
  }
}

// Bật bơm nước và đặt hẹn giờ tắt
void startWaterPump() {
  // Bật bơm nước
  digitalWrite(WATER_PUMP_PIN, HIGH);
  wateringStartTime = millis();
  
  // Lấy thời gian hiện tại
  struct tm timeinfo;
  char currentTimeStr[9]; // HH:MM:SS\0
  
  if (getLocalTime(&timeinfo)) {
    strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M:%S", &timeinfo);
  } else {
    strcpy(currentTimeStr, "Không xác định");
  }
  
  // Thông báo bắt đầu tưới nước
  String message = "💧 *BẮT ĐẦU TƯỚI NƯỚC TỰ ĐỘNG* 💧\n\n";
  message += "⏰ *Thời gian bắt đầu tưới*: " + String(currentTimeStr) + "\n";
  message += "⏱️ *Thời lượng tưới*: " + String(PUMP_DURATION / 1000) + " giây\n";
  
  bot.sendMessage(CHAT_ID_1, message, "Markdown");
  bot.sendMessage(CHAT_ID_2, message, "Markdown");
  
  Serial.println("Bắt đầu tưới nước tự động, thời gian hiện tại: " + String(currentTimeStr));
}

// Function to start watering pump via web interface
void handleStartWaterPump() {
  Serial.println("Starting water pump via web interface");
  
  // Call the existing startWaterPump function
  startWaterPump();
  
  // Send success response
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Water pump started\"}");
}

// Function to set scheduled watering time via web interface
void handleSetScheduledWateringTime() {
  if (server.hasArg("time")) {
    String timeArg = server.arg("time");
    Serial.print("Setting scheduled watering time to: ");
    Serial.println(timeArg);
    
    // Set the global scheduledWateringTime variable
    scheduledWateringTime = timeArg;
    wateringScheduleActive = true;  // Activate the watering schedule
    alreadyWateredToday = false;    // Reset watering status
    
    // Send success response
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Watering time scheduled\"}");
    
    // Send notification to Telegram
    String message = "⏰ *WATERING SCHEDULE UPDATED* ⏰\n\n";
    message += "New watering time set: " + timeArg;
    
    bot.sendMessage(CHAT_ID_1, message, "Markdown");
    bot.sendMessage(CHAT_ID_2, message, "Markdown");
  } else {
    // Bad request - missing time parameter
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing time parameter\"}");
  }
}

// Function to handle device status request
void handleStatus() {
  Serial.println("Status requested from web interface");
  
  // Get current time
  struct tm timeinfo;
  char timeStr[30] = "Unknown";
  char dateStr[30] = "Unknown";
  char uptimeStr[30] = "Unknown";
  
  if (getLocalTime(&timeinfo)) {
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
  }
  
  // Calculate uptime
  unsigned long uptime = millis() / 1000;
  sprintf(uptimeStr, "%d days, %d hours, %d minutes", 
    (int)(uptime / 86400), 
    (int)((uptime % 86400) / 3600), 
    (int)((uptime % 3600) / 60));
  
  // Create JSON response
  String jsonResponse = "{";
  jsonResponse += "\"status\":\"Online\",";
  jsonResponse += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  jsonResponse += "\"ssid\":\"" + String(ssid) + "\",";
  jsonResponse += "\"time\":\"" + String(timeStr) + "\",";
  jsonResponse += "\"date\":\"" + String(dateStr) + "\",";
  jsonResponse += "\"uptime\":\"" + String(uptimeStr) + "\",";
  jsonResponse += "\"free_heap\":\"" + String(ESP.getFreeHeap()) + " bytes\",";
  jsonResponse += "\"api_url\":\"http://" + WiFi.localIP().toString() + "\"";
  jsonResponse += "}";
  
  server.send(200, "application/json", jsonResponse);
  
  Serial.println("Status sent via web interface");
}

// Function to handle the predict endpoint
void handlePredict() {
  Serial.println("Prediction requested from web interface");
  
  // Get the current disease prediction status
  String jsonResponse = "{";
  jsonResponse += "\"predicted_class\":\"" + (predictedDisease == "Không có" ? "Healthy" : predictedDisease) + "\",";
  jsonResponse += "\"confidence\":\"85%\","; // Example confidence
  jsonResponse += "\"disease\":\"" + (predictedDisease == "Không có" ? "None" : predictedDisease) + "\"";
  jsonResponse += "}";
  
  server.send(200, "application/json", jsonResponse);
  
  Serial.println("Prediction sent via web interface");
}

// Function to handle receiving disease data from ESP32-CAM
void handleReceiveDisease() {
  Serial.println("Receiving disease data from ESP32-CAM");
  
  // Check if we have the predicted_class parameter
  if (server.hasArg("predicted_class")) {
    String newDisease = server.arg("predicted_class");
    
    // Update our disease status
    predictedDisease = (newDisease == "Healthy" || newDisease == "healthy") ? "Không có" : newDisease;
    lastDiseaseUpdateTime = millis();
    
    Serial.print("Updated disease status: ");
    Serial.println(predictedDisease);
    
    // Create response
    String jsonResponse = "{";
    jsonResponse += "\"status\":\"success\",";
    jsonResponse += "\"message\":\"Disease data updated\",";
    jsonResponse += "\"disease\":\"" + predictedDisease + "\"";
    jsonResponse += "}";
    
    server.send(200, "application/json", jsonResponse);
    
    // If we have a disease, get treatment recommendations
    if (predictedDisease != "Không có") {
      // Send notification to Telegram
      String message = "🔬 *BỆNH MỚI PHÁT HIỆN* 🔬\n\n";
      message += "🌱 *Loại bệnh*: " + predictedDisease + "\n";
      message += "⏰ *Thời gian phát hiện*: " + String(millis() / 1000) + " giây từ khi khởi động\n\n";
      message += "Đang lấy khuyến nghị điều trị...";
      
      bot.sendMessage(CHAT_ID_1, message, "Markdown");
      bot.sendMessage(CHAT_ID_2, message, "Markdown");
      
      // Get treatment recommendations
      getTreatmentFromGemini(predictedDisease);
    }
  } else {
    // No predicted_class parameter provided
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing predicted_class parameter\"}");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo cảm biến DHT
  dht.begin();
  
  // Cấu hình chân cảm biến và điều khiển
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, LOW); // Tắt bơm nước khi khởi động
  
  // Kết nối WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/update-to-sheets", HTTP_GET, handleUpdateToSheets);
  server.on("/report", HTTP_GET, handleDailyReport);
  server.on("/report-results", HTTP_GET, handleReportResults);
  server.on("/analysis", HTTP_GET, handleDetailedAnalysis);
  server.on("/analysis-results", HTTP_GET, handleAnalysisResults);
  
  // Add new handlers for device status, predictions, and water control
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/predict", HTTP_GET, handlePredict);
  server.on("/startWaterPump", HTTP_POST, handleStartWaterPump);
  server.on("/setScheduledWateringTime", HTTP_POST, handleSetScheduledWateringTime);
  server.on("/receive-disease", HTTP_POST, handleReceiveDisease);
  
  server.begin();
  Serial.println("HTTP server started");
  
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
  
  // Gửi dữ liệu cảm biến lần đầu
  sendDataToGoogleSheets();

  // Lấy dự báo thời tiết và hỏi Gemini
  getWeatherAndAskGemini();
  
  // Thông báo hệ thống đã sẵn sàng
  String startupMsg = "🚀 *HỆ THỐNG ĐÃ KHỞI ĐỘNG* 🚀\n\n";
  startupMsg += "✅ Kết nối WiFi thành công\n";
  startupMsg += "✅ Đồng bộ thời gian thành công\n";
  startupMsg += "✅ Gửi dữ liệu lên Google Sheets đã sẵn sàng\n";
  startupMsg += "✅ Dự báo thời tiết và phân tích đã hoạt động\n\n";
  startupMsg += "Hệ thống sẽ tự động gửi dữ liệu mỗi 5 phút và báo cáo hàng ngày lúc 23:00.";
  
  bot.sendMessage(CHAT_ID_1, startupMsg, "Markdown");
  bot.sendMessage(CHAT_ID_2, startupMsg, "Markdown");
}

// Kiểm tra và thực hiện tưới nước tự động
bool checkAndWater() {
  // Kiểm tra xem đã có thời gian tưới nước được đặt chưa
  if (!wateringScheduleActive || scheduledWateringTime.length() == 0) {
    return false;
  }
  
  // Lấy thời gian hiện tại
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Không thể lấy thời gian hiện tại");
    return false;
  }
  
  // Định dạng thời gian hiện tại thành HH:MM để so sánh
  char currentTimeStr[6]; // HH:MM\0
  strftime(currentTimeStr, sizeof(currentTimeStr), "%H:%M", &timeinfo);
  String currentTime = String(currentTimeStr);
  
  // Lấy ngày hiện tại để kiểm tra reset trạng thái tưới
  char currentDateStr[11]; // YYYY-MM-DD\0
  strftime(currentDateStr, sizeof(currentDateStr), "%Y-%m-%d", &timeinfo);
  static String lastWateringDate = "";
  
  // Reset trạng thái tưới nước khi sang ngày mới
  if (lastWateringDate != String(currentDateStr)) {
    lastWateringDate = String(currentDateStr);
    alreadyWateredToday = false;
  }
  
  // Kiểm tra xem đã đến thời gian tưới nước chưa và chưa tưới hôm nay
  if (currentTime == scheduledWateringTime && !alreadyWateredToday) {
    int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
    float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    
    // Kiểm tra độ ẩm đất trước khi tưới
    if (soilMoistureValue > SOIL_THRESHOLD || soilMoisturePercent < 35) {
      // Đất khô, cần tưới nước
      startWaterPump();
      return true;
    } else {
      // Đất đủ ẩm, không cần tưới nước
      String message = "🌧️ *KIỂM TRA TƯỚI NƯỚC TỰ ĐỘNG* 🌧️\n\n";
      message += "⏰ *Thời gian kiểm tra*: " + currentTime + "\n";
      message += "🌱 *Độ ẩm đất hiện tại*: " + String(soilMoisturePercent, 0) + "%\n\n";
      message += "✅ Độ ẩm đất đủ cao, không cần tưới nước.\n";
      
      bot.sendMessage(CHAT_ID_1, message, "Markdown");
      bot.sendMessage(CHAT_ID_2, message, "Markdown");
      
      // Đánh dấu đã kiểm tra tưới nước hôm nay
      alreadyWateredToday = true;
      return false;
    }
  }
  
  return false;
}

// Kiểm tra và tắt bơm nước sau khi hết thời gian
void checkAndStopPump() {
  if (digitalRead(WATER_PUMP_PIN) == HIGH) {
    if (millis() - wateringStartTime >= PUMP_DURATION) {
      // Tắt bơm nước
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
      
      bot.sendMessage(CHAT_ID_1, message, "Markdown");
      bot.sendMessage(CHAT_ID_2, message, "Markdown");
      
      Serial.println("Kết thúc tưới nước tự động, thời gian: " + String(currentTimeStr));
    }
  }
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
  if (currentMillis - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNewMessages) {
      Serial.println("Có tin nhắn mới!");
      for (int i = 0; i < numNewMessages; i++) {
        String chat_id = bot.messages[i].chat_id;
        String text = bot.messages[i].text;
        
        if (text == "/status") {
          // Đọc dữ liệu cảm biến hiện tại
          float temperature = dht.readTemperature();
          float humidity = dht.readHumidity();
          
          // Đọc độ ẩm đất
          int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
          float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
          soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
          
          String status = "📊 *TRẠNG THÁI HIỆN TẠI* 📊\n\n";
          status += "🌡️ *Nhiệt độ*: " + String(temperature, 1) + " °C\n";
          status += "💧 *Độ ẩm không khí*: " + String(humidity, 1) + " %\n";
          status += "🌱 *Độ ẩm đất*: " + String(soilMoisturePercent, 0) + " %\n\n";
          
          // Thêm thông tin về lịch tưới nước
          if (wateringScheduleActive && scheduledWateringTime.length() > 0) {
            status += "⏰ *Lịch tưới nước*: " + scheduledWateringTime + "\n";
            status += "🚿 *Trạng thái tưới*: ";
            if (alreadyWateredToday) {
              status += "Đã tưới hôm nay\n";
            } else {
              status += "Chưa tưới hôm nay\n";
            }
          } else {
            status += "⏰ *Lịch tưới nước*: Chưa được đặt\n";
          }
          
          bot.sendMessage(chat_id, status, "Markdown");
        } else if (text == "/update") {
          bot.sendMessage(chat_id, "Đang cập nhật dữ liệu lên Google Sheets...");
          sendDataToGoogleSheets();
        } else if (text == "/report") {
          bot.sendMessage(chat_id, "Đang tạo báo cáo hàng ngày...");
          sendDailyReport();
        } else if (text == "/analysis") {
          bot.sendMessage(chat_id, "Đang yêu cầu phân tích chi tiết từ Gemini...");
          requestDetailedAnalysis();
        } else if (text == "/weather") {
          bot.sendMessage(chat_id, "Đang cập nhật dự báo thời tiết...");
          getWeatherAndAskGemini();
        } else if (text == "/test") {
          bot.sendMessage(chat_id, "Đang kiểm tra kết nối Google Script...");
          testGoogleScriptConnection();
        } else if (text == "/water") {
          bot.sendMessage(chat_id, "Đang kích hoạt tưới nước thủ công...");
          startWaterPump();
        } else if (text == "/soil") {
          // Đọc độ ẩm đất
          int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
          float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
          soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
          
          String soilStatus = "🌱 *TRẠNG THÁI ĐỘ ẨM ĐẤT* 🌱\n\n";
          soilStatus += "📊 *Giá trị ADC*: " + String(soilMoistureValue) + "\n";
          soilStatus += "💧 *Phần trăm độ ẩm*: " + String(soilMoisturePercent, 0) + " %\n\n";
          
          if (soilMoistureValue > SOIL_THRESHOLD) {
            soilStatus += "⚠️ *Đánh giá*: Đất khô, cần tưới nước";
          } else {
            soilStatus += "✅ *Đánh giá*: Đất đủ ẩm, không cần tưới nước";
          }
          
          bot.sendMessage(chat_id, soilStatus, "Markdown");
        } else if (text == "/disease") {
          // Truy vấn thông tin bệnh đã phát hiện
          String message = "🔍 *THÔNG TIN BỆNH ĐÃ PHÁT HIỆN* 🔍\n\n";
          
          if (predictedDisease == "Không có") {
            message += "✅ Chưa phát hiện bệnh nào gần đây.";
          } else {
            message += "🌱 *Loại bệnh*: " + predictedDisease + "\n";
            
            // Thời gian phát hiện (tính từ thời điểm nhận được)
            unsigned long timeSinceDetection = (millis() - lastDiseaseUpdateTime) / 1000; // Đổi sang giây
            
            if (timeSinceDetection < 60) {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection) + " giây trước\n\n";
            } else if (timeSinceDetection < 3600) {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection / 60) + " phút trước\n\n";
            } else if (timeSinceDetection < 86400) {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection / 3600) + " giờ trước\n\n";
            } else {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection / 86400) + " ngày trước\n\n";
            }
            
            // Thêm nhắc nhở
            message += "⚠️ *Lưu ý*: Vui lòng kiểm tra cây trồng của bạn và thực hiện các biện pháp phòng ngừa phù hợp.";
          }
          
          bot.sendMessage(chat_id, message, "Markdown");
        } else if (text == "/help") {
          String helpText = "📱 *LỆNH ĐIỀU KHIỂN* 📱\n\n";
          helpText += "/status - Xem trạng thái cảm biến hiện tại\n";
          helpText += "/update - Cập nhật dữ liệu lên Google Sheets\n";
          helpText += "/report - Tạo báo cáo hàng ngày từ Google Sheets\n";
          helpText += "/analysis - Yêu cầu phân tích chi tiết từ Gemini\n";
          helpText += "/weather - Cập nhật dự báo thời tiết\n";
          helpText += "/test - Kiểm tra kết nối Google Script\n";
          helpText += "/water - Kích hoạt tưới nước thủ công\n";
          helpText += "/soil - Kiểm tra độ ẩm đất hiện tại\n";
          helpText += "/disease - Xem thông tin bệnh được phát hiện gần đây\n";
          helpText += "/help - Hiển thị danh sách lệnh\n";
          
          bot.sendMessage(chat_id, helpText, "Markdown");
        } else if (text.startsWith("/setreporttime ")) {
          // Định dạng lệnh: /setreporttime 23:00
          String timeStr = text.substring(14);
          int separatorPos = timeStr.indexOf(":");
          
          if (separatorPos > 0) {
            int hour = timeStr.substring(0, separatorPos).toInt();
            int minute = timeStr.substring(separatorPos + 1).toInt();
            
            if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {
              dailyReportHour = hour;
              dailyReportMinute = minute;
              
              String response = "✅ Đã đặt thời gian báo cáo hàng ngày thành " + 
                               String(dailyReportHour) + ":" + 
                               (dailyReportMinute < 10 ? "0" : "") + String(dailyReportMinute);
              
              bot.sendMessage(chat_id, response, "");
            } else {
              bot.sendMessage(chat_id, "❌ Thời gian không hợp lệ. Sử dụng định dạng: /setreporttime 23:00", "");
            }
          } else {
            bot.sendMessage(chat_id, "❌ Định dạng không hợp lệ. Sử dụng: /setreporttime 23:00", "");
          }
        } else if (text.startsWith("/setwater ")) {
          // Định dạng lệnh: /setwater 17:00
          String timeStr = text.substring(10);
          int separatorPos = timeStr.indexOf(":");
          
          if (separatorPos > 0) {
            // Cập nhật thời gian tưới nước thủ công
            scheduledWateringTime = timeStr;
            wateringScheduleActive = true;
            alreadyWateredToday = false;
            
            String response = "✅ Đã đặt thời gian tưới nước thành " + scheduledWateringTime;
            bot.sendMessage(chat_id, response, "");
            
            Serial.println("Đã đặt lịch tưới nước thủ công: " + scheduledWateringTime);
          } else {
            bot.sendMessage(chat_id, "❌ Định dạng không hợp lệ. Sử dụng: /setwater 17:00", "");
          }
        }
      }
    }
    lastTimeBotRan = currentMillis;
  }
}