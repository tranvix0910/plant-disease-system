#include "tele_bot.h"

UniversalTelegramBot bot(BOTtoken, client);

int botRequestDelay = 1000;
unsigned long lastTimeBotRan;
const long messageInterval = 5000;
unsigned long lastMessageTime = 0;

int dailyReportHour = 23;
int dailyReportMinute = 0;
bool reportSentToday = false;

extern unsigned long lastDiseaseUpdateTime;

void telegramBotSetup(){
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
}

void sendToAllChannels(const String& message, const String& format = "Markdown") {
    bot.sendMessage(CHAT_ID_1, message, format);
    bot.sendMessage(CHAT_ID_2, message, format);
}

void messageCheck(){
    if (millis() - lastTimeBotRan > botRequestDelay) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNewMessages) {
      Serial.println("Có tin nhắn mới!");
      for (int i = 0; i < numNewMessages; i++) {
        String chat_id = bot.messages[i].chat_id;
        String text = bot.messages[i].text;
        
        if (text == "/status") {

          float temperature = readTemperature();
          float humidity = readHumidity();
          
          float soilMoisturePercent = readSoilMoisturePercent();
          
          String status = "📊 *TRẠNG THÁI HIỆN TẠI* 📊\n\n";
          status += "🌡️ *Nhiệt độ*: " + String(temperature, 1) + " °C\n";
          status += "💧 *Độ ẩm không khí*: " + String(humidity, 1) + " %\n";
          status += "🌱 *Độ ẩm đất*: " + String(soilMoisturePercent, 0) + " %\n\n";
          
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
          String message = "🔍 *THÔNG TIN BỆNH ĐÃ PHÁT HIỆN* 🔍\n\n";
          
          if (predictedDisease == "Không có") {
            message += "✅ Chưa phát hiện bệnh nào gần đây.";
          } else {
            message += "🌱 *Loại bệnh*: " + predictedDisease + "\n";
            
            unsigned long timeSinceDetection = (millis() - lastDiseaseUpdateTime) / 1000;
            
            if (timeSinceDetection < 60) {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection) + " giây trước\n\n";
            } else if (timeSinceDetection < 3600) {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection / 60) + " phút trước\n\n";
            } else if (timeSinceDetection < 86400) {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection / 3600) + " giờ trước\n\n";
            } else {
              message += "🕒 *Thời gian phát hiện*: " + String(timeSinceDetection / 86400) + " ngày trước\n\n";
            }
            
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
    lastTimeBotRan = millis();
  }
}


