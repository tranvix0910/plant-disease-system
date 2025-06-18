#include "gemini.h"
#include "promts.h"

const char* latitude = "10.7769";
const char* longitude = "106.7009";
const char* timezone = "Asia/Ho_Chi_Minh";
char reason[10000];

String predictedDisease = "Không có";

String scheduledWateringTime = "";
bool wateringScheduleActive = false;
bool alreadyWateredToday = false;

String extractValue(String jsonText, String key) {
  int keyPos = jsonText.indexOf("\"" + key + "\"");
  if (keyPos == -1) {
    return "Not found";
  }
  
  int valueStart = jsonText.indexOf("\"", keyPos + key.length() + 3) + 1;
  if (valueStart == 0) {
    return "Not found";
  }
  
  int valueEnd = jsonText.indexOf("\"", valueStart);
  if (valueEnd == -1) {
    return "Not found";
  }
  
  return jsonText.substring(valueStart, valueEnd);
}

void sendErrorMessage(String errorType, String errorDetail, String diseaseName) {
  String errorMsg = "❌ *" + errorType + "* ❌\n\n";
  errorMsg += errorDetail + "\n\n";
  
  if (diseaseName != "Không có" && diseaseName != "Healthy" && diseaseName != "healthy") {
    errorMsg += "Đối với bệnh " + diseaseName + ", đề xuất chung:\n";
    errorMsg += "• Tách cây bị bệnh ra khỏi khu vực\n";
    errorMsg += "• Loại bỏ bộ phận cây bị nhiễm\n";
    errorMsg += "• Tưới nước vào buổi sáng sớm (5-7h)\n";
    errorMsg += "• Tránh bón phân đạm quá nhiều\n";
    errorMsg += "• Tham khảo ý kiến chuyên gia nông nghiệp\n";
  }
  
  sendToAllChannels(errorMsg, "Markdown");
}

static void fillPromptPlaceholders(String& prompt, const JsonObject& summary, const String& dataPoints) {
  struct { const char* key; String value; } vars[] = {
    {"{{date}}", String(summary["date"].as<const char*>())},
    {"{{readings}}", String(summary["readings"].as<const char*>())},
    {"{{avgTemp}}", String(summary["avgTemp"].as<float>(), 1)},
    {"{{maxTemp}}", String(summary["maxTemp"].as<float>(), 1)},
    {"{{minTemp}}", String(summary["minTemp"].as<float>(), 1)},
    {"{{avgHumidity}}", String(summary["avgHumidity"].as<float>(), 1)},
    {"{{avgSoilMoisture}}", String(summary["avgSoilMoisture"].as<float>(), 1)},
    {"{{maxSoilMoisture}}", String(summary["maxSoilMoisture"].as<float>(), 1)},
    {"{{minSoilMoisture}}", String(summary["minSoilMoisture"].as<float>(), 1)},
    {"{{dataPoints}}", dataPoints}
  };
  for (auto& v : vars) prompt.replace(v.key, v.value); 
}

static String buildDataPoints(const JsonArray& dataArray) {
  String dataPoints;
  int limit = std::min(8, (int)dataArray.size());
  int step = dataArray.size() > 0 ? std::max(1, (int)dataArray.size() / limit) : 1;
  for (int i = 0; i < dataArray.size(); i += step) {
    if (dataPoints.length() > 500) {
      dataPoints += "- ... và " + String(dataArray.size() - i) + " điểm dữ liệu khác\n";
      break;
    }
    JsonObject point = dataArray[i].as<JsonObject>();
    dataPoints += "- Thời gian: " + String(point["time"].as<const char*>()) + ", ";
    dataPoints += "Nhiệt độ: " + String(point["temperature"].as<float>(), 1) + "°C, ";
    dataPoints += "Độ ẩm không khí: " + String(point["humidity"].as<float>(), 0) + "%, ";
    dataPoints += "Độ ẩm đất: " + String(point["soil_moisture"].as<float>(), 0) + "%\n";
  }
  return dataPoints;
}

void sendDetailedReportToGemini(JsonObject& summary) {
  String dataPoints = buildDataPoints(summary["data"].as<JsonArray>());
  String prompt = String(PROMPT_DETAILED_ANALYSIS);
  fillPromptPlaceholders(prompt, summary, dataPoints);

  sendToAllChannels("🔍 *ĐANG PHÂN TÍCH DỮ LIỆU*\n\nVui lòng đợi trong giây lát...", "Markdown");

  WiFiClientSecure gemini_client;
  gemini_client.setInsecure();
  gemini_client.setTimeout(30);

  HTTPClient https;
  https.setConnectTimeout(30000);
  https.setTimeout(30000);

  String gemini_url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(GEMINI_API_KEY);
  
  if (!https.begin(gemini_client, gemini_url)) return;
  
    https.addHeader("Content-Type", "application/json");
    prompt.replace("\"", "\\\"");

    String payload = "{\"contents\": [{\"parts\":[{\"text\":\"" + prompt + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": 1024, \"temperature\": 0.4}}";
    
    int httpCode = https.POST(payload);
    
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    lastAnalysisResults = "❌ Error: Could not connect to Gemini API.";
    analysisInProgress = false;
    sendToAllChannels(lastAnalysisResults, "Markdown");
    https.end();
    return;
  }

      String response = https.getString();
  https.end();
  DynamicJsonDocument doc(16384);

  if (deserializeJson(doc, response)) {
    lastAnalysisResults = "❌ Error: Could not parse Gemini response.";
    analysisInProgress = false;
    sendToAllChannels(lastAnalysisResults, "Markdown");
    return;
  }
        String analysis = doc["candidates"][0]["content"]["parts"][0]["text"];
        analysis.trim();
        String reportMessage = "📊 *BÁO CÁO PHÂN TÍCH* 📊\n\n";
  reportMessage += "📅 *Ngày*: " + String(summary["date"].as<const char*>()) + "\n";
  reportMessage += "🌡️ *Nhiệt độ*: " + String(summary["avgTemp"].as<float>(), 1) + "°C (TB), " + String(summary["maxTemp"].as<float>(), 1) + "°C (Max), " + String(summary["minTemp"].as<float>(), 1) + "°C (Min)\n";
  reportMessage += "💧 *Độ ẩm không khí TB*: " + String(summary["avgHumidity"].as<float>(), 1) + "%\n";
  reportMessage += "🌱 *Độ ẩm đất TB*: " + String(summary["avgSoilMoisture"].as<float>(), 1) + "%\n\n";
        analysis.replace("Nhiệt độ", "🌡️ Nhiệt độ");
        analysis.replace("Độ ẩm không khí", "💧 Độ ẩm không khí");
        analysis.replace("Độ ẩm đất", "🌱 Độ ẩm đất");
        analysis.replace("Phân tích", "📊 Phân tích");
        analysis.replace("Khuyến nghị", "💡 Khuyến nghị");
        analysis.replace("Lưu ý", "⚠️ Lưu ý");
        analysis.replace("Kết luận", "✅ Kết luận");
        analysis.replace("Dự báo", "🔮 Dự báo");
        analysis.replace("Rủi ro", "⚠️ Rủi ro");
        lastAnalysisResults = reportMessage + analysis;
        analysisInProgress = false;
        const int MAX_MESSAGE_SIZE = 3800;
        if (analysis.length() <= MAX_MESSAGE_SIZE) {
    sendToAllChannels(lastAnalysisResults, "Markdown");
        } else {
    sendToAllChannels(reportMessage, "Markdown");
          int msgCount = (analysis.length() + MAX_MESSAGE_SIZE - 1) / MAX_MESSAGE_SIZE;
          for (int i = 0; i < msgCount; i++) {
            int startPos = i * MAX_MESSAGE_SIZE;
            int endPos = min((i + 1) * MAX_MESSAGE_SIZE, (int)analysis.length());
            String part = analysis.substring(startPos, endPos);
            String partHeader = "*PHẦN " + String(i + 1) + "/" + String(msgCount) + "*\n\n";
      sendToAllChannels(partHeader + part, "Markdown");
            delay(500);
          }
        }
}

void getWeatherAndAskGemini() {
  WiFiClientSecure client;
  client.setInsecure();

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
    if (!deserializeJson(doc, payload)) {
      JsonObject current = doc["current"];
      weather_summary += "=== Current Weather ===\n";
      weather_summary += "Temperature: " + String(current["temperature_2m"].as<float>()) + " °C\n";
      weather_summary += "Humidity: " + String(current["relative_humidity_2m"].as<int>()) + "%\n";
      weather_summary += "Pressure: " + String(current["pressure_msl"].as<float>()) + " hPa\n";
      weather_summary += "Wind Speed: " + String(current["wind_speed_10m"].as<float>()) + " m/s\n\n";
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
    } else {
      Serial.print("JSON parse error: ");
      Serial.println(payload);
      http.end();
      return;
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
    http.end();
    return;
  }
  http.end();

  bool hasDisease = (predictedDisease != "Healthy");
  String prompt = String(PROMPT_WEATHER_DISEASE);
  prompt.replace("{{role_intro}}", hasDisease
    ? "Với vai trò là chuyên gia nông nghiệp về bệnh cây trồng, hãy phân tích dữ liệu thời tiết sau đây:"
    : "Với vai trò là trợ lý nông nghiệp thông minh, hãy phân tích dữ liệu thời tiết sau đây:");
  prompt.replace("{{weather_summary}}", weather_summary);
  prompt.replace("{{disease_section}}", hasDisease
    ? String("\n===== THÔNG TIN BỆNH CÂY =====\nLoại bệnh đã phát hiện: ") + predictedDisease + "\n"
    : "");
  prompt.replace("{{analysis_requirements}}", hasDisease
    ? String("1. Dựa vào các biến số thời tiết (nhiệt độ, độ ẩm, lượng mưa, gió) và tình trạng bệnh của cây, xác định thời điểm tối ưu để bón phân cho cây trồng trong 7 ngày tới, lưu ý đến việc cây đang bị bệnh ") + predictedDisease + ".\n"
      "2. Xác định CHÍNH XÁC MỘT GIỜ cụ thể (VD: 17:00 hoặc 6:30) lý tưởng để tưới cây cà chua trong ngày hôm nay, có tính đến các yếu tố thời tiết, sinh lý cây trồng, và đặc biệt là bệnh " + predictedDisease + " của cây.\n"
      "3. Giải thích lý do cho những đề xuất trên (dựa trên các nguyên tắc nông học và kiến thức về bệnh cây).\n"
      "4. Đề xuất cách điều trị bệnh kết hợp với lịch tưới nước và bón phân.\n"
    : "1. Dựa vào các biến số thời tiết (nhiệt độ, độ ẩm, lượng mưa, gió), xác định thời điểm tối ưu để bón phân cho cây trồng trong 7 ngày tới.\n"
      "2. Xác định CHÍNH XÁC MỘT GIỜ cụ thể (VD: 17:00 hoặc 6:30) lý tưởng để tưới cây cà chua trong ngày hôm nay, có tính đến các yếu tố thời tiết và sinh lý cây trồng.\n"
      "3. Giải thích lý do cho những đề xuất trên (dựa trên các nguyên tắc nông học).\n");
  prompt.replace("{{treatment_field}}", hasDisease ? ",\n  \"treatment\": \"đề xuất điều trị cụ thể cho bệnh " + predictedDisease + " kết hợp với lịch tưới nước và bón phân\"" : "");

  WiFiClientSecure gemini_client;
  gemini_client.setInsecure();
  HTTPClient https;
  String gemini_url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(GEMINI_API_KEY);
  if (!https.begin(gemini_client, gemini_url)) return;
  https.addHeader("Content-Type", "application/json");
  prompt.replace("\"", "\\\"");
  String payload = "{\"contents\": [{\"parts\":[{\"text\":\"" + prompt + "\"}]}],\"generationConfig\": {\"maxOutputTokens\": 1024, \"temperature\": 0.4}}";
  int httpCode = https.POST(payload);
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    Serial.println("[Gemini] API request failed");
    https.end();
    return;
  }
  String response = https.getString();
  https.end();
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, response)) {
    Serial.println("[Gemini] JSON parse error");
    return;
  }
  String Answer = doc["candidates"][0]["content"]["parts"][0]["text"];
  Answer.trim();
  Serial.println("\n===== Gemini Suggestion =====");
  Serial.println(Answer);
  String wateringTime = extractValue(Answer, "tomato_watering_time");
  String fertilizingDay = extractValue(Answer, "best_fertilization_day");
  String reason = extractValue(Answer, "reason");
  scheduledWateringTime = wateringTime;
  wateringScheduleActive = true;
  alreadyWateredToday = false;
  Serial.print("Đã đặt lịch tưới nước lúc: ");
  Serial.println(scheduledWateringTime);
  String formattedMessage;
  if (hasDisease) {
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
  sendToAllChannels(formattedMessage, "Markdown");
}

void getTreatmentFromGemini(String diseaseName) {
  WiFiClientSecure gemini_client;
  gemini_client.setInsecure();

  float temperature = readTemperature();
  float humidity = readHumidity();

  struct tm timeinfo;
  char timeStr[30] = "Không xác định";
  char dateStr[30] = "Không xác định";
  if (getLocalTime(&timeinfo)) {
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
  }

  String prompt = PROMPT_TREATMENT_ANALYSIS;

  // Build dynamic prompt sections
  String role_intro, env_info, disease_section, requirements;
  if (diseaseName == "Healthy" || diseaseName == "healthy" || diseaseName == "Không có") {
    role_intro = "Bạn là chuyên gia nông nghiệp cho cây cà chua. Hiện tại cây không phát hiện bệnh.";
    disease_section = "";
    requirements =
      "1. Thời gian tưới nước lý tưởng trong ngày (giờ cụ thể)\n"
      "2. Khuyến nghị về tần suất và lượng nước tưới\n"
      "3. Đề xuất lịch bón phân phù hợp (thời gian và loại phân)\n"
      "4. Các biện pháp phòng bệnh phù hợp với điều kiện hiện tại";
      } else {
    role_intro = "Bạn là chuyên gia nông nghiệp về bệnh cây trồng. Tôi phát hiện cây cà chua có bệnh: '" + diseaseName + "'.";
    disease_section = "\nBệnh phát hiện: '" + diseaseName + "'";
    requirements =
      "1. Mô tả ngắn về bệnh và mức độ nguy hiểm\n"
      "2. Biện pháp điều trị cụ thể, ưu tiên biện pháp hữu cơ\n"
      "3. THỜI GIAN TƯỚI NƯỚC PHÙ HỢP (giờ cụ thể, ví dụ: 17:00) để không làm trầm trọng thêm bệnh\n"
      "4. THỜI GIAN BÓN PHÂN PHÙ HỢP và loại phân nên dùng hoặc tránh khi cây bị bệnh này\n"
      "5. Biện pháp phòng ngừa lâu dài";
  }
  env_info = "Điều kiện môi trường hiện tại: nhiệt độ " + String(temperature, 1) + "°C, độ ẩm " + String(humidity, 1) + "%, thời gian hiện tại là " + String(timeStr) + " ngày " + String(dateStr) + ".";

  prompt.replace("{{role_intro}}", role_intro);
  prompt.replace("{{env_info}}", env_info);
  prompt.replace("{{disease_section}}", disease_section);
  prompt.replace("{{requirements}}", requirements);

  HTTPClient https;
  Serial.println("Gửi yêu cầu phân tích đến Gemini...");

  if (https.begin(gemini_client, "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + String(GEMINI_API_KEY))) {
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
        String messageTitle, messageIcon;
        if (diseaseName == "Không có" || diseaseName == "Healthy" || diseaseName == "healthy") {
          messageTitle = "LỊCH CHĂM SÓC TỐI ƯU";
          messageIcon = "🌿";
        } else {
          messageTitle = "PHÂN TÍCH BỆNH & LỊCH CHĂM SÓC";
          messageIcon = "🔬";
        }
        String detailedMessage = messageIcon + " *" + messageTitle + "* " + messageIcon + "\n\n";
        if (diseaseName != "Không có" && diseaseName != "Healthy" && diseaseName != "healthy") {
          detailedMessage += "🌱 *Loại bệnh*: " + diseaseName + "\n";
        }
        detailedMessage += "🌡️ *Nhiệt độ*: " + String(temperature, 1) + "°C\n";
        detailedMessage += "💧 *Độ ẩm*: " + String(humidity, 1) + "%\n";
        detailedMessage += "🕒 *Thời gian*: " + String(timeStr) + " - " + String(dateStr) + "\n\n";
        detailedMessage += analysis;
        sendToAllChannels(detailedMessage, "Markdown");
      } else {
        sendErrorMessage("LỖI PHÂN TÍCH", "Không thể phân tích phản hồi JSON từ Gemini.", diseaseName);
      }
    } else {
      sendErrorMessage("LỖI KẾT NỐI", "Không thể kết nối đến dịch vụ Gemini (HTTP code: " + String(httpCode) + ").", diseaseName);
    }
    https.end();
  } else {
    sendErrorMessage("LỖI KẾT NỐI", "Không thể thiết lập kết nối đến dịch vụ Gemini.", diseaseName);
  }
}