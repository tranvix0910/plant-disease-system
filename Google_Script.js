// Google Apps Script để ghi dữ liệu cảm biến vào Google Sheets

function doGet(e) {
  Logger.log("--- doGet ---");
  Logger.log("Request parameters: " + JSON.stringify(e ? e.parameters : "none"));
  
  try {
    // Đảm bảo e không bị undefined
    if (!e || !e.parameters) {
      Logger.log("Không có tham số, sử dụng giá trị mặc định");
      e = { 
        parameters: {
          date: new Date(),
          time: Utilities.formatDate(new Date(), "GMT+7", "HH:mm:ss"),
          temperature: "28.5",
          humidity: "65.2",
          soil_moisture: "45.0"
        }
      };
    }
    
    // Nếu là yêu cầu kiểm tra kết nối
    if (e.parameters.action && (e.parameters.action == "test" || (Array.isArray(e.parameters.action) && e.parameters.action[0] == "test"))) {
      Logger.log("Nhận yêu cầu kiểm tra kết nối");
      return ContentService.createTextOutput(JSON.stringify({
        status: "success",
        message: "Kết nối đến Google Script thành công",
        timestamp: new Date().toISOString(),
        appVersion: "1.1.0"
      })).setMimeType(ContentService.MimeType.JSON);
    }
    
    // Nếu là yêu cầu lấy báo cáo hàng ngày
    if (e.parameters.action && e.parameters.action == "getDailyReport") {
      var dateToReport = e.parameters.date;
      
      // Kiểm tra và định dạng ngày
      if (dateToReport instanceof Date) {
        dateToReport = Utilities.formatDate(dateToReport, "GMT+7", "yyyy-MM-dd");
      } else if (Array.isArray(dateToReport)) {
        // Nếu là mảng (có thể xảy ra với query params), lấy phần tử đầu tiên
        dateToReport = dateToReport[0];
      } else if (typeof dateToReport === 'string' && dateToReport.trim() === '') {
        // Nếu không cung cấp ngày, sử dụng ngày hiện tại
        dateToReport = Utilities.formatDate(new Date(), "GMT+7", "yyyy-MM-dd");
      } else if (!dateToReport) {
        // Nếu không có ngày, sử dụng ngày hiện tại
        dateToReport = Utilities.formatDate(new Date(), "GMT+7", "yyyy-MM-dd");
      }
      
      return generateDailyReport(dateToReport);
    }
    
    // Nếu không, xử lý như bình thường với dữ liệu cảm biến
    var date = "";
    var time = "";
    var temperature = "";
    var humidity = "";
    var soil_moisture = "";
    
    // Lấy các tham số từ request
    if (e.parameters.date) {
      date = e.parameters.date;
      // Nếu là mảng (có thể xảy ra với query params), lấy phần tử đầu tiên
      if (Array.isArray(date)) date = date[0];
    }
    
    if (e.parameters.time) {
      time = e.parameters.time;
      if (Array.isArray(time)) time = time[0];
    }
    
    if (e.parameters.temperature) {
      temperature = e.parameters.temperature;
      if (Array.isArray(temperature)) temperature = temperature[0];
    }
    
    if (e.parameters.humidity) {
      humidity = e.parameters.humidity;
      if (Array.isArray(humidity)) humidity = humidity[0];
    }
    
    // Thêm tham số độ ẩm đất
    if (e.parameters.soil_moisture) {
      soil_moisture = e.parameters.soil_moisture;
      if (Array.isArray(soil_moisture)) soil_moisture = soil_moisture[0];
    }
    
    // Nếu date là đối tượng Date, định dạng lại thành chuỗi
    if (date instanceof Date) {
      date = Utilities.formatDate(date, "GMT+7", "yyyy-MM-dd");
    }
    
    // Nếu time là đối tượng Date, định dạng lại thành chuỗi
    if (time instanceof Date) {
      time = Utilities.formatDate(time, "GMT+7", "HH:mm:ss");
    }
    
    // Lưu dữ liệu vào Google Sheets
    saveData(date, time, temperature, humidity, soil_moisture);
    
    // Trả về kết quả dạng JSON
    return ContentService.createTextOutput(JSON.stringify({
      status: "success",
      message: "Đã ghi dữ liệu thành công",
      data: {
        date: date,
        time: time,
        temperature: temperature,
        humidity: humidity,
        soil_moisture: soil_moisture
      },
      timestamp: new Date().toISOString()
    })).setMimeType(ContentService.MimeType.JSON);
    
  } catch(error) { 
    Logger.log("Lỗi trong doGet: " + error.message);
    Logger.log(error.stack);
    
    // Trả về lỗi dạng JSON
    return ContentService.createTextOutput(JSON.stringify({
      status: "error",
      message: error.message,
      timestamp: new Date().toISOString()
    })).setMimeType(ContentService.MimeType.JSON);
  }  
}

// Tạo báo cáo hàng ngày và trả về JSON
function generateDailyReport(dateStr) {
  Logger.log("--- generateDailyReport for " + dateStr + " ---");
  
  // Kiểm tra xem dateStr có phải là ngày hợp lệ không
  var targetDate = new Date(dateStr);
  var isValidDate = !isNaN(targetDate.getTime());
  
  Logger.log("Input date: " + dateStr);
  Logger.log("Parsed date: " + targetDate);
  Logger.log("Is valid date: " + isValidDate);
  
  try {
    // Mở spreadsheet
    var ss = SpreadsheetApp.openByUrl("https://docs.google.com/spreadsheets/d/1TL3eZKGvPJPkzvwfWgkRNlIFvacSC1WcySUlwyRMPnA/edit");
    var sheet = ss.getSheetByName("Sheet1");
    
    if (!sheet) {
      // Không tìm thấy sheet
      return ContentService.createTextOutput(JSON.stringify({
        error: "Không tìm thấy sheet"
      })).setMimeType(ContentService.MimeType.JSON);
    }
    
    // Lấy tất cả dữ liệu
    var dataRange = sheet.getDataRange();
    var values = dataRange.getValues();
    
    // Nếu không có dữ liệu
    if (values.length <= 1) {
      return ContentService.createTextOutput(JSON.stringify({
        error: "Không có dữ liệu"
      })).setMimeType(ContentService.MimeType.JSON);
    }
    
    // PHƯƠNG PHÁP MỚI: Tìm các dòng dữ liệu theo chuỗi ngày
    // 1. Chuẩn bị chuỗi ngày cần tìm (yyyy-MM-dd)
    var targetDateStr = "";
    if (isValidDate) {
      targetDateStr = Utilities.formatDate(targetDate, "GMT+7", "yyyy-MM-dd");
    } else {
      // Nếu không phải Date hợp lệ, sử dụng chuỗi nguyên bản
      targetDateStr = dateStr;
    }
    
    Logger.log("Tìm kiếm dữ liệu theo chuỗi ngày: " + targetDateStr);
    
    // 2. Chuẩn bị mảng chứa dữ liệu khớp
    var dailyData = [];
    
    // 3. Lặp qua từng dòng dữ liệu để tìm kiếm
    for (var i = 1; i < values.length; i++) { // Bỏ qua hàng tiêu đề
      var rowDate = values[i][0]; // Cột Date
      var rowDateStr = "";
      
      // Chuyển đổi rowDate thành chuỗi có định dạng yyyy-MM-dd
      if (rowDate instanceof Date) {
        rowDateStr = Utilities.formatDate(rowDate, "GMT+7", "yyyy-MM-dd");
      } else if (typeof rowDate === 'string') {
        rowDateStr = rowDate;
      } else {
        rowDateStr = String(rowDate);
      }
      
      Logger.log("Dòng " + i + " - Ngày: " + rowDateStr + " (từ " + rowDate + ")");
      
      // So sánh chuỗi ngày
      if (rowDateStr === targetDateStr) {
        Logger.log("  → MATCH: chuỗi ngày khớp!");
        
        // Lấy dữ liệu từ dòng
        var timeValue = values[i][1] || "";
        var temperature = values[i][2] || 0;
        var humidity = values[i][3] || 0;
        var soil_moisture = values[i][4] || 0; // Lấy thêm giá trị độ ẩm đất
        
        var timeStr = "";
        if (timeValue instanceof Date) {
          timeStr = Utilities.formatDate(timeValue, "GMT+7", "HH:mm:ss");
        } else {
          timeStr = String(timeValue);
        }
        
        Logger.log("  → Thời gian: " + timeStr);
        Logger.log("  → Nhiệt độ: " + temperature);
        Logger.log("  → Độ ẩm không khí: " + humidity);
        Logger.log("  → Độ ẩm đất: " + soil_moisture);
        
        // Chuyển đổi dấu phẩy thành dấu chấm cho nhiệt độ, độ ẩm không khí và độ ẩm đất
        var tempStr = String(temperature).replace(',', '.');
        var humidStr = String(humidity).replace(',', '.');
        var soilMoistureStr = String(soil_moisture).replace(',', '.');
        
        dailyData.push({
          date: rowDateStr,
          time: timeStr,
          temperature: parseFloat(tempStr) || 0,
          humidity: parseFloat(humidStr) || 0,
          soil_moisture: parseFloat(soilMoistureStr) || 0
        });
      }
    }
    
    // Tổng kết kết quả
    Logger.log("Tổng số bản ghi khớp với ngày " + targetDateStr + ": " + dailyData.length);
    
    // Nếu không có dữ liệu cho ngày này
    if (dailyData.length == 0) {
      Logger.log("Không tìm thấy dữ liệu cho ngày " + targetDateStr);
      return ContentService.createTextOutput(JSON.stringify({
        error: "Không có dữ liệu cho ngày " + targetDateStr
      })).setMimeType(ContentService.MimeType.JSON);
    }
    
    Logger.log("Tìm thấy " + dailyData.length + " bản ghi cho ngày " + targetDateStr);
    
    // Tính toán các giá trị thống kê
    var totalTemp = 0;
    var maxTemp = -999;
    var minTemp = 999;
    var totalHumidity = 0;
    var totalSoilMoisture = 0;
    var maxSoilMoisture = -999;
    var minSoilMoisture = 999;
    
    for (var i = 0; i < dailyData.length; i++) {
      var temp = dailyData[i].temperature;
      var humidity = dailyData[i].humidity;
      var soil_moisture = dailyData[i].soil_moisture;
      
      totalTemp += temp;
      totalHumidity += humidity;
      totalSoilMoisture += soil_moisture;
      
      if (temp > maxTemp) maxTemp = temp;
      if (temp < minTemp) minTemp = temp;
      
      if (soil_moisture > maxSoilMoisture) maxSoilMoisture = soil_moisture;
      if (soil_moisture < minSoilMoisture) minSoilMoisture = soil_moisture;
    }
    
    var avgTemp = totalTemp / dailyData.length;
    var avgHumidity = totalHumidity / dailyData.length;
    var avgSoilMoisture = totalSoilMoisture / dailyData.length;
    
    // Log thông tin chi tiết
    Logger.log("Thống kê cho ngày " + targetDateStr + ":");
    Logger.log(" - Số lượng đo: " + dailyData.length);
    Logger.log(" - Nhiệt độ TB: " + avgTemp.toFixed(1) + "°C");
    Logger.log(" - Nhiệt độ max: " + maxTemp.toFixed(1) + "°C");
    Logger.log(" - Nhiệt độ min: " + minTemp.toFixed(1) + "°C");
    Logger.log(" - Độ ẩm không khí TB: " + avgHumidity.toFixed(1) + "%");
    Logger.log(" - Độ ẩm đất TB: " + avgSoilMoisture.toFixed(1) + "%");
    Logger.log(" - Độ ẩm đất max: " + maxSoilMoisture.toFixed(1) + "%");
    Logger.log(" - Độ ẩm đất min: " + minSoilMoisture.toFixed(1) + "%");
    
    // Tạo đối tượng tóm tắt
    var summary = {
      date: targetDateStr,
      readings: dailyData.length,
      avgTemp: avgTemp,
      maxTemp: maxTemp,
      minTemp: minTemp,
      avgHumidity: avgHumidity,
      avgSoilMoisture: avgSoilMoisture,
      maxSoilMoisture: maxSoilMoisture,
      minSoilMoisture: minSoilMoisture,
      data: dailyData
    };
    
    // Trả về JSON
    return ContentService.createTextOutput(JSON.stringify({
      summary: summary
    })).setMimeType(ContentService.MimeType.JSON);
    
  } catch(error) {
    Logger.log("Lỗi báo cáo: " + JSON.stringify(error));
    return ContentService.createTextOutput(JSON.stringify({
      error: error.message
    })).setMimeType(ContentService.MimeType.JSON);
  }
}

// Hàm lưu dữ liệu vào Google Sheets
function saveData(date, time, temperature, humidity, soil_moisture) {
  Logger.log("--- saveData ---"); 
  
  try {
    // Mở spreadsheet - THAY ĐỔI URL ĐẾN GOOGLE SHEET CỦA BẠN
    var ss = SpreadsheetApp.openByUrl("https://docs.google.com/spreadsheets/d/1TL3eZKGvPJPkzvwfWgkRNlIFvacSC1WcySUlwyRMPnA/edit");
    var sheet = ss.getSheetByName("Sheet1"); // Đảm bảo tên sheet trùng với sheet của bạn
    
    if (!sheet) {
      // Nếu sheet không tồn tại, tạo mới sheet
      sheet = ss.insertSheet("Sheet1");
      
      // Thêm tiêu đề
      sheet.getRange("A1").setValue("Date");
      sheet.getRange("B1").setValue("Time");
      sheet.getRange("C1").setValue("Temperature");
      sheet.getRange("D1").setValue("Humidity");
      sheet.getRange("E1").setValue("Soil Moisture");
      
      // Định dạng tiêu đề
      sheet.getRange("A1:E1").setFontWeight("bold");
      sheet.setFrozenRows(1);
    } else {
      // Kiểm tra xem cột Soil Moisture đã tồn tại chưa
      var headers = sheet.getRange(1, 1, 1, 10).getValues()[0];
      var soilMoistureColExists = headers.indexOf("Soil Moisture") !== -1;
      
      // Thêm cột Soil Moisture nếu chưa tồn tại
      if (!soilMoistureColExists) {
        var lastCol = headers.filter(String).length + 1;
        sheet.getRange(1, lastCol).setValue("Soil Moisture");
        sheet.getRange(1, lastCol).setFontWeight("bold");
      }
    }
    
    // Lấy dòng cuối cùng
    var lastRow = sheet.getLastRow() + 1;
    
    // Thêm dữ liệu mới
    sheet.getRange("A" + lastRow).setValue(date);
    sheet.getRange("B" + lastRow).setValue(time);
    sheet.getRange("C" + lastRow).setValue(parseFloat(temperature));
    sheet.getRange("D" + lastRow).setValue(parseFloat(humidity));
    
    // Thêm dữ liệu độ ẩm đất
    var soilMoistureCol = "E"; // Mặc định là cột E
    
    // Nếu sheet đã có sẵn, cần xác định đúng cột Soil Moisture
    if (lastRow > 2) {
      var headers = sheet.getRange(1, 1, 1, 10).getValues()[0];
      var soilMoistureIndex = headers.indexOf("Soil Moisture");
      if (soilMoistureIndex !== -1) {
        // Chuyển đổi index thành chữ cái cột (0 -> A, 1 -> B, etc.)
        soilMoistureCol = String.fromCharCode(65 + soilMoistureIndex);
      }
    }
    
    sheet.getRange(soilMoistureCol + lastRow).setValue(parseFloat(soil_moisture));
    
    // Định dạng cột nhiệt độ, độ ẩm không khí và độ ẩm đất để hiển thị 1 chữ số thập phân
    sheet.getRange("C" + lastRow).setNumberFormat("0.0");
    sheet.getRange("D" + lastRow).setNumberFormat("0.0");
    sheet.getRange(soilMoistureCol + lastRow).setNumberFormat("0.0");
    
    // Cập nhật ô thông tin gần nhất (nếu có)
    if (sheet.getRange("F1").getValue() != "" && sheet.getRange("F1").getValue() == "Cập nhật gần nhất:") {
      sheet.getRange("G1").setValue(new Date());
    }
    
    Logger.log("Đã lưu dữ liệu vào dòng " + lastRow);
  }
  catch(error) {
    Logger.log("Lỗi: " + JSON.stringify(error));
    throw error;
  }
  
  Logger.log("--- saveData kết thúc ---"); 
}

// Hàm kiểm tra kết nối - Sử dụng để kiểm tra khi triển khai
function testConnection() {
  var testData = {
    parameters: {
      date: "2023-05-21",
      time: "14:30:00",
      temperature: "28.5",
      humidity: "65.2",
      soil_moisture: "45.0"
    }
  };
  
  doGet(testData);
}

// Kiểm tra tính năng tạo báo cáo hàng ngày
function testDailyReport() {
  // Tạo ngày hiện tại ở định dạng YYYY-MM-DD
  var today = new Date();
  var dateStr = Utilities.formatDate(today, "GMT+7", "yyyy-MM-dd");
  
  // Log thông tin về ngày đang kiểm tra
  Logger.log("Kiểm tra báo cáo cho ngày: " + dateStr);
  
  // Parse thử dateStr thành Date để kiểm tra tính hợp lệ
  var testDate = new Date(dateStr);
  Logger.log("Ngày được parse: " + testDate + ", hợp lệ: " + !isNaN(testDate.getTime()));
  
  var testData = {
    parameters: {
      action: "getDailyReport",
      date: dateStr
    }
  };
  
  var result = doGet(testData);
  Logger.log(result.getContent());
}

// Kiểm tra tính năng tạo báo cáo cho ngày cụ thể
function testDailyReportForDate(dateStr) {
  // Nếu không cung cấp ngày, sử dụng ngày mặc định để test
  if (!dateStr) {
    dateStr = "2025-05-12";
  }
  
  // Log thông tin về ngày đang kiểm tra
  Logger.log("Kiểm tra báo cáo cho ngày cụ thể: " + dateStr);
  
  // Parse thử dateStr thành Date để kiểm tra tính hợp lệ
  var testDate = new Date(dateStr);
  Logger.log("Ngày được parse: " + testDate + ", hợp lệ: " + !isNaN(testDate.getTime()));
  
  var testData = {
    parameters: {
      action: "getDailyReport",
      date: dateStr
    }
  };
  
  var result = doGet(testData);
  Logger.log(result.getContent());
} 