#include "reports.h"
#include "../gemini/promts.h"

DynamicJsonDocument lastDailyReport(8192);

int dailyReportHour = 23;
int dailyReportMinute = 0;
bool reportSentToday = false;

String lastReportResults = "";
bool reportInProgress = false;
unsigned long reportStartTime = 0;

String lastAnalysisResults = "";
bool analysisInProgress = false;
unsigned long analysisStartTime = 0;

bool isTimeToSendDailyReport() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Không thể lấy thời gian");
    return false;
  }
  
  if (timeinfo.tm_hour == dailyReportHour && 
      timeinfo.tm_min == dailyReportMinute && 
      !reportSentToday) {
    return true;
  }
  
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
    reportSentToday = false;
  }
  
  return false;
}

void saveDailyReport(JsonObject summary) {
    lastDailyReport.clear();
    lastDailyReport.set(summary);   
}

String buildDailyReportMessage(const JsonObject& summary, const char* dateStr) {
    float avgTemp = summary["avgTemp"];
    float maxTemp = summary["maxTemp"];
    float minTemp = summary["minTemp"];
    float avgHumidity = summary["avgHumidity"];
    float avgSoilMoisture = summary["avgSoilMoisture"];
    float maxSoilMoisture = summary["maxSoilMoisture"];
    float minSoilMoisture = summary["minSoilMoisture"];
    int readings = summary["readings"];

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
    // Phân tích đơn giản
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
    return message;
}

void sendDailyReport() {
    Serial.println("Đang tạo báo cáo hàng ngày...");
    reportInProgress = true;
    reportStartTime = millis();
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Không thể lấy thời gian");
        reportInProgress = false;
        lastReportResults = "❌ Error: Could not get current time.";
        return;
    }
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    String urlFinal = "https://script.google.com/macros/s/"+String(GOOGLE_SCRIPT_ID)+"/exec?";
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
        if (payload.startsWith("{")) {
            DynamicJsonDocument doc(8192);
            DeserializationError error = deserializeJson(doc, payload);
            if (!error) {
                if (doc.containsKey("summary")) {
                    String message = buildDailyReportMessage(doc["summary"], dateStr);
                    lastReportResults = message;
                    reportInProgress = false;
                    sendToAllChannels(message);
                    reportSentToday = true;
                    Serial.println("Đã gửi báo cáo hàng ngày thành công");
                    saveDailyReport(doc["summary"]);
                } else if (doc.containsKey("error")) {
                    String errorMsg = doc["error"].as<String>();
                    String message = "❌ *KHÔNG CÓ DỮ LIỆU BÁO CÁO* ❌\n\n";
                    message += errorMsg;
                    lastReportResults = message;
                    reportInProgress = false;
                    sendToAllChannels(message);
                    Serial.println("Lỗi từ Google Script: " + errorMsg);
                } else {
                    String message = "❌ *KHÔNG CÓ DỮ LIỆU BÁO CÁO* ❌\n\n";
                    message += "Không có dữ liệu cho ngày " + String(dateStr);
                    lastReportResults = message;
                    reportInProgress = false;
                    sendToAllChannels(message);
                    Serial.println("Không có dữ liệu cho báo cáo hàng ngày");
                }
            } else {
                Serial.println("Lỗi phân tích JSON: " + String(error.c_str()));
                Serial.println("Dữ liệu nhận được: " + payload);
                lastReportResults = "❌ Lỗi phân tích dữ liệu báo cáo!";
                reportInProgress = false;
                sendToAllChannels("❌ Lỗi phân tích dữ liệu báo cáo!", "");
            }
        } else {
            Serial.println("Phản hồi không phải là JSON hợp lệ: " + payload);
            lastReportResults = "❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!";
            reportInProgress = false;
            sendToAllChannels("❌ Lỗi: Phản hồi từ máy chủ không đúng định dạng JSON!", "");
        }
    } else {
        Serial.println("Lỗi kết nối HTTP, code: " + String(httpCode));
        lastReportResults = "❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode);
        reportInProgress = false;
        sendToAllChannels("❌ Lỗi kết nối đến Google Sheets để lấy báo cáo! Code: " + String(httpCode), "");
    }
    http.end();
}

void requestDetailedAnalysis() {
  if (lastDailyReport.size() > 0) {
    analysisInProgress = true;
    analysisStartTime = millis();
    lastAnalysisResults = "";
    
    JsonObject summary = lastDailyReport.as<JsonObject>();
    sendDetailedReportToGemini(summary);
  } else {
    lastAnalysisResults = "❌ No recent report data available for analysis.";
    sendToAllChannels("❌ Không có dữ liệu báo cáo gần đây để phân tích!", "Markdown");
  }
}
