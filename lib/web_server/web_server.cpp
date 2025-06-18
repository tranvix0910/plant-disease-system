#include "web_server.h"

extern DynamicJsonDocument lastDailyReport(8192);

float latestTemperature = 0;
float latestHumidity = 0;
float latestSoilMoisturePercent = 0;

String lastReportResults = "";
bool reportInProgress = false;
unsigned long reportStartTime = 0;

String lastAnalysisResults = "";
bool analysisInProgress = false;
unsigned long analysisStartTime = 0;

unsigned long lastDiseaseUpdateTime = 0;

void handleRoot() {
  String html = String(PLANT_MONITOR_HTML);
  html.replace("{{TEMPERATURE}}", isnan(latestTemperature) ? "--" : String(latestTemperature, 1));
  html.replace("{{HUMIDITY}}", isnan(latestHumidity) ? "--" : String(latestHumidity, 1));
  html.replace("{{SOIL_MOISTURE}}", String(latestSoilMoisturePercent, 1));
  server.send(200, "text/html", html);
}

void handleUpdate() {

  float temperature = readTemperature();
  float humidity = readHumidity();
  int soilMoistureValue = readSoilMoisture();
  float soilMoisturePercent = map(soilMoistureValue, DRY_SOIL, WET_SOIL, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  
  latestTemperature = temperature;
  latestHumidity = humidity;
  latestSoilMoisturePercent = soilMoisturePercent;
  
  String jsonResponse = "{";
  jsonResponse += "\"temperature\":" + String(temperature, 1) + ",";
  jsonResponse += "\"humidity\":" + String(humidity, 1) + ",";
  jsonResponse += "\"soil_moisture\":" + String(soilMoisturePercent, 1) + ",";
  jsonResponse += "\"pump_status\":\"" + String(waterPumpRead() == HIGH ? "on" : "off") + "\",";
  jsonResponse += "\"next_watering_time\":\"" + scheduledWateringTime + "\"";
  jsonResponse += "}";
  
  server.send(200, "application/json", jsonResponse);
  
  Serial.println("Data updated via web interface");
}

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

void handleUpdateToSheets() {
  Serial.println("Sending data to Google Sheets via web interface");
  
  sendDataToGoogleSheets();
  
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Data sent to Google Sheets\"}");
}

void handleDailyReport() {
  Serial.println("Daily report requested from web interface");
  
  if (lastReportResults.length() > 0) {
    server.send(200, "text/plain", lastReportResults);
    return;
  }
  
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

  sendDailyReport();
  
  reportBuffer += "\nA full report has been sent to Telegram.";

  server.send(200, "text/plain", reportBuffer);
  
  Serial.println("Daily report sent to web client and Telegram");
}

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

void handleStartWaterPump() {
  Serial.println("Starting water pump via web interface");
  
  // Call the existing startWaterPump function
  startWaterPump();
  
  // Send success response
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Water pump started\"}");
}

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

void handleReceiveDisease() {
  Serial.println("Receiving disease data from ESP32-CAM");
  
  if (server.hasArg("predicted_class")) {
    String newDisease = server.arg("predicted_class");
    
    predictedDisease = (newDisease == "Healthy" || newDisease == "healthy") ? "Không có" : newDisease;
    lastDiseaseUpdateTime = millis();
    
    Serial.print("Updated disease status: ");
    Serial.println(predictedDisease);
    
    String jsonResponse = "{";
    jsonResponse += "\"status\":\"success\",";
    jsonResponse += "\"message\":\"Disease data updated\",";
    jsonResponse += "\"disease\":\"" + predictedDisease + "\"";
    jsonResponse += "}";
    
    server.send(200, "application/json", jsonResponse);
    
    if (predictedDisease != "Không có") {
      String message = "🔬 *BỆNH MỚI PHÁT HIỆN* 🔬\n\n";
      message += "🌱 *Loại bệnh*: " + predictedDisease + "\n";
      message += "⏰ *Thời gian phát hiện*: " + String(millis() / 1000) + " giây từ khi khởi động\n\n";
      message += "Đang lấy khuyến nghị điều trị...";
      
      bot.sendMessage(CHAT_ID_1, message, "Markdown");
      bot.sendMessage(CHAT_ID_2, message, "Markdown");
      
      getTreatmentFromGemini(predictedDisease);
    }
  } else {
    server.send(
      400, 
      "application/json", 
      "{\"status\":\"error\",\"message\":\"Missing predicted_class parameter\"}"
    );
  }
}

void webServerSetup() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/update-to-sheets", HTTP_GET, handleUpdateToSheets);
  server.on("/report", HTTP_GET, handleDailyReport);
  server.on("/report-results", HTTP_GET, handleReportResults);
  server.on("/analysis", HTTP_GET, handleDetailedAnalysis);
  server.on("/analysis-results", HTTP_GET, handleAnalysisResults);
  
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/predict", HTTP_GET, handlePredict);
  server.on("/startWaterPump", HTTP_POST, handleStartWaterPump);
  server.on("/setScheduledWateringTime", HTTP_POST, handleSetScheduledWateringTime);
  server.on("/receive-disease", HTTP_POST, handleReceiveDisease);
  
  server.begin();
}

