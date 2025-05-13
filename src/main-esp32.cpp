#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>
#include <time.h>
#include <WebServer.h>

// Định nghĩa chân cảm biến
#define DHTPIN 2      // Chân kết nối DHT11
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

const char* ssid = "311HHN Lau 1";
const char* password = "@@1234abcdlau1";
const char* Gemini_Token = "AIzaSyA3ogt7LgUlDTuHqtMPZsFFompKnuYADAw";
const char* Gemini_Max_Tokens = "10000";
#define BOTtoken "7729298728:AAGwPQvhVE8sc9FlNHDDSLqUU8WLVzt-0QU"
#define CHAT_ID_1 "5797970828"
#define CHAT_ID_2 "1281777025"

String GOOGLE_SCRIPT_ID = "AKfycbwCfxuDEZO0xSfDD4Qnm5dkXGz1yojbaY9wlmmbrBELmWvp8Rv7q3YDaMUwOVHG1jsqtg"; 
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
    dataPoints += "Độ ẩm: " + String(point["humidity"].as<float>(), 0) + "%\n";
  }
  
  // Tạo prompt hỏi Gemini - làm ngắn gọn để giảm kích thước
  String prompt = "Bạn là chuyên gia nông nghiệp, phân tích dữ liệu cảm biến sau:\n\n";
  prompt += "Ngày: " + date + "\n";
  prompt += "Số đo: " + String(readings) + "\n";
  prompt += "Nhiệt độ: TB=" + String(avgTemp, 1) + "°C, Max=" + String(maxTemp, 1) + "°C, Min=" + String(minTemp, 1) + "°C\n";
  prompt += "Độ ẩm TB: " + String(avgHumidity, 1) + "%\n\n";
  
  prompt += "Dữ liệu mẫu:\n";
  prompt += dataPoints + "\n";
  
  prompt += "Yêu cầu:\n";
  prompt += "1. Phân tích môi trường (nhiệt độ, độ ẩm) trong ngày.\n";
  prompt += "2. Biến động nhiệt độ và độ ẩm trong ngày.\n";
  prompt += "3. Đánh giá mức độ phù hợp cho cây cà chua.\n";
  prompt += "4. Đề xuất biện pháp tối ưu điều kiện trồng trọt.\n";
  prompt += "5. Dự báo rủi ro sâu bệnh, nấm mốc.\n\n";
  
  prompt += "Trình bày ngắn gọn, chuyên nghiệp, tối đa 800 từ.";
  
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
        reportMessage += "💧 *Độ ẩm TB*: " + String(avgHumidity, 1) + "%\n\n";
        
        // Thay thế các từ khóa bằng emojis để làm báo cáo sinh động hơn
        analysis.replace("Nhiệt độ", "🌡️ Nhiệt độ");
        analysis.replace("Độ ẩm", "💧 Độ ẩm");
        analysis.replace("Phân tích", "📊 Phân tích");
        analysis.replace("Khuyến nghị", "💡 Khuyến nghị");
        analysis.replace("Lưu ý", "⚠️ Lưu ý");
        analysis.replace("Kết luận", "✅ Kết luận");
        analysis.replace("Dự báo", "🔮 Dự báo");
        analysis.replace("Rủi ro", "⚠️ Rủi ro");
        
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
        bot.sendMessage(CHAT_ID_1, "❌ Lỗi: Không thể phân tích phản hồi từ Gemini.", "Markdown");
        bot.sendMessage(CHAT_ID_2, "❌ Lỗi: Không thể phân tích phản hồi từ Gemini.", "Markdown");
      }
    } else {
      Serial.printf("[HTTPS] POST error: %s\n", https.errorToString(httpCode).c_str());
      
      // Phân tích đơn giản nếu Gemini timeout
      String fallbackMessage = "📊 *BÁO CÁO PHÂN TÍCH CƠ BẢN* 📊\n\n";
      fallbackMessage += "📅 *Ngày*: " + date + "\n";
      fallbackMessage += "🌡️ *Nhiệt độ*: " + String(avgTemp, 1) + "°C (TB), " + String(maxTemp, 1) + "°C (Max), " + String(minTemp, 1) + "°C (Min)\n";
      fallbackMessage += "💧 *Độ ẩm TB*: " + String(avgHumidity, 1) + "%\n\n";
      
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
      
      // Phân tích độ ẩm
      if (avgHumidity > 80) {
        fallbackMessage += "💧 *Độ ẩm cao*: Độ ẩm trung bình (" + String(avgHumidity, 1) + "%) cao hơn mức tối ưu (65-75%). Rủi ro cao về các bệnh nấm và mốc. Cần cải thiện thông gió và giảm tưới nước.\n\n";
      } else if (avgHumidity < 50) {
        fallbackMessage += "💧 *Độ ẩm thấp*: Độ ẩm trung bình (" + String(avgHumidity, 1) + "%) thấp hơn mức tối ưu (65-75%). Cần tăng cường tưới nước và che phủ đất.\n\n";
      } else {
        fallbackMessage += "💧 *Độ ẩm phù hợp*: Độ ẩm trung bình (" + String(avgHumidity, 1) + "%) gần với khoảng tối ưu (65-75%) cho cây cà chua.\n\n";
      }
      
      // Khuyến nghị
      fallbackMessage += "💡 *KHUYẾN NGHỊ*:\n";
      
      if (avgTemp > 30 && avgHumidity > 80) {
        fallbackMessage += "- Cải thiện thông gió để giảm độ ẩm\n";
        fallbackMessage += "- Tưới nước vào buổi sáng sớm\n";
        fallbackMessage += "- Theo dõi các dấu hiệu bệnh nấm\n";
        fallbackMessage += "- Phun thuốc phòng bệnh nếu cần\n";
      } else if (avgTemp > 30 && avgHumidity < 60) {
        fallbackMessage += "- Tăng tưới nước, tối ưu vào sáng sớm và chiều tối\n";
        fallbackMessage += "- Che phủ đất để giữ ẩm\n";
        fallbackMessage += "- Tạo bóng râm cho cây trong những giờ nắng gắt\n";
      } else if (avgTemp < 20) {
        fallbackMessage += "- Sử dụng màng phủ để giữ nhiệt\n";
        fallbackMessage += "- Tưới nước vào buổi trưa khi ấm nhất\n";
        fallbackMessage += "- Cân nhắc các biện pháp tăng nhiệt nếu có thể\n";
      }
      
      fallbackMessage += "\n⚠️ *Lưu ý*: Đây là phân tích cơ bản do kết nối đến Gemini không thành công. Để có phân tích chi tiết, hãy thử lại sau.";
      
      bot.sendMessage(CHAT_ID_1, fallbackMessage, "Markdown");
      bot.sendMessage(CHAT_ID_2, fallbackMessage, "Markdown");
    }
    https.end();
  } else {
    Serial.println("[HTTPS] Không thể kết nối đến Gemini");
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
  
  // Lấy thời gian hiện tại
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Không thể lấy thời gian");
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
          int readings = doc["summary"]["readings"];
          
          // Tạo tin nhắn báo cáo
          String message = "📊 *BÁO CÁO HÀNG NGÀY* 📊\n\n";
          message += "📅 *Ngày*: " + String(dateStr) + "\n";
          message += "🔢 *Số đo*: " + String(readings) + " lần\n\n";
          message += "🌡️ *Nhiệt độ*:\n";
          message += "  • Trung bình: " + String(avgTemp, 1) + " °C\n";
          message += "  • Cao nhất: " + String(maxTemp, 1) + " °C\n";
          message += "  • Thấp nhất: " + String(minTemp, 1) + " °C\n\n";
          message += "💧 *Độ ẩm trung bình*: " + String(avgHumidity, 1) + " %\n\n";
          
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
            message += "⚠️ Độ ẩm cao, có thể dẫn đến nấm bệnh\n";
          } else if (avgHumidity < 40) {
            message += "⚠️ Độ ẩm thấp, cần tăng cường tưới nước\n";
          } else {
            message += "✅ Độ ẩm phù hợp cho cây phát triển\n";
          }
          
          message += "\n📎 Đường dẫn đến báo cáo đầy đủ:\nhttps://docs.google.com/spreadsheets/d/1TL3eZKGvPJPkzvwfWgkRNlIFvacSC1WcySUlwyRMPnA/edit";
          message += "\n\n💡 Để xem phân tích chi tiết, gửi lệnh /analysis";
          
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
          
          bot.sendMessage(CHAT_ID_1, message, "Markdown");
          bot.sendMessage(CHAT_ID_2, message, "Markdown");
          Serial.println("Lỗi từ Google Script: " + errorMsg);
        } else {
          // Không có dữ liệu
          String message = "❌ *KHÔNG CÓ DỮ LIỆU BÁO CÁO* ❌\n\n";
          message += "Không có dữ liệu cho ngày " + String(dateStr);
          
          bot.sendMessage(CHAT_ID_1, message, "Markdown");
          bot.sendMessage(CHAT_ID_2, message, "Markdown");
          Serial.println("Không có dữ liệu cho báo cáo hàng ngày");
        }
      } else {
        Serial.println("Lỗi phân tích JSON: " + String(error.c_str()));
        Serial.println("Dữ liệu nhận được: " + payload);
        bot.sendMessage(CHAT_ID_1, "❌ Lỗi phân tích dữ liệu báo cáo!", "");
        bot.sendMessage(CHAT_ID_2, "❌ Lỗi phân tích dữ liệu báo cáo!", "");
      }
    } else {
      Serial.println("Phản hồi không phải là JSON hợp lệ: " + payload);
      bot.sendMessage(CHAT_ID_1, "❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!", "");
      bot.sendMessage(CHAT_ID_2, "❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!", "");
    }
  } else {
    Serial.println("Lỗi kết nối HTTP, code: " + String(httpCode));
    bot.sendMessage(CHAT_ID_1, "❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode), "");
    bot.sendMessage(CHAT_ID_2, "❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode), "");
  }
  
  http.end();
}

// Yêu cầu phân tích chi tiết từ Gemini về báo cáo đã lưu
void requestDetailedAnalysis() {
  if (lastDailyReport.size() > 0) {
    JsonObject summary = lastDailyReport.as<JsonObject>();
    sendDetailedReportToGemini(summary);
  } else {
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

// Xử lý khi nhận dữ liệu bệnh từ ESP32-CAM
void handleDiseaseRequest() {
  if (server.hasArg("name")) {
    predictedDisease = server.arg("name");
    predictedDisease.replace("_", " "); // Chuyển Tomato_Blight thành "Tomato Blight"
    lastDiseaseUpdateTime = millis();
    
    Serial.println("Nhận bệnh dự đoán từ ESP32-CAM: " + predictedDisease);
    
    // Thông báo qua Telegram - phần thông tin cơ bản
    String initialMessage = "🔍 *ĐANG XỬ LÝ PHÁT HIỆN BỆNH* 🔍\n\n";
    initialMessage += "🌱 *Loại bệnh*: " + predictedDisease + "\n";
    initialMessage += "🕒 *Thời gian*: ";
    
    // Thêm thông tin thời gian
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[30];
      strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
      initialMessage += String(timeStr) + "\n\n";
    } else {
      initialMessage += "Không xác định\n\n";
    }
    
    initialMessage += "⏳ Đang phân tích và chuẩn bị khuyến nghị chi tiết...";
    
    // Gửi thông báo ban đầu qua Telegram
    bot.sendMessage(CHAT_ID_1, initialMessage, "Markdown");
    bot.sendMessage(CHAT_ID_2, initialMessage, "Markdown");
    
    // Gửi OK response để ESP32-CAM biết là nhận thành công
    server.send(200, "text/plain", "OK");
    
    // Gọi Gemini để xin khuyến nghị chi tiết
    getTreatmentFromGemini(predictedDisease);
  } else {
    server.send(400, "text/plain", "Bad Request: Missing 'name' parameter");
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
  
  // Thiết lập route cho server
  server.on("/disease", HTTP_GET, handleDiseaseRequest);
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
      
      // Thông báo kết thúc tưới nước
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
  // Xử lý các yêu cầu từ client
  server.handleClient();
  
  // Kiểm tra và thực hiện tưới nước tự động
  checkAndWater();
  
  // Kiểm tra và tắt bơm nước sau khi hết thời gian tưới
  checkAndStopPump();
  
  // Gửi dữ liệu cảm biến định kỳ (5 phút/lần)
  unsigned long currentMillis = millis();
  
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
