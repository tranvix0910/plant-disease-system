#pragma once

const char* PROMPT_DETAILED_ANALYSIS = R"(
Bạn là chuyên gia nông nghiệp, phân tích dữ liệu cảm biến sau:
Ngày: {{date}}
Số đo: {{readings}}
Nhiệt độ: TB={{avgTemp}}°C, Max={{maxTemp}}°C, Min={{minTemp}}°C
Độ ẩm không khí TB: {{avgHumidity}}%
Độ ẩm đất: TB={{avgSoilMoisture}}%, Max={{maxSoilMoisture}}%, Min={{minSoilMoisture}}%

Dữ liệu mẫu:
{{dataPoints}}

Yêu cầu:
1. Phân tích môi trường (nhiệt độ, độ ẩm không khí, độ ẩm đất) trong ngày.
2. Biến động nhiệt độ và độ ẩm trong ngày.
3. Đánh giá mức độ phù hợp cho cây cà chua dựa trên các thông số trên.
4. Đề xuất biện pháp tối ưu điều kiện trồng trọt, đặc biệt là lịch tưới nước dựa trên độ ẩm đất.
5. Dự báo rủi ro sâu bệnh, nấm mốc.

Trình bày ngắn gọn, chuyên nghiệp, tối đa 250 từ.
)";

const char* PROMPT_WEATHER_DISEASE = R"(
{{role_intro}}
===== THÔNG TIN THỜI TIẾT =====
{{weather_summary}}
{{disease_section}}
===== YÊU CẦU PHÂN TÍCH =====
{{analysis_requirements}}
===== YÊU CẦU ĐỊNH DẠNG PHẢN HỒI =====
Trả lời dưới dạng JSON theo cấu trúc sau (không có chữ ở ngoài):
'tomato_watering_time': 'giờ cụ thể để tưới cây cà chua hôm nay, PHẢI là một thời điểm cụ thể theo định dạng HH:MM (ví dụ: 17:00, 6:00, 18:30)',
'best_fertilization_day': 'ngày tốt nhất để bón phân trong 7 ngày tới, định dạng ngày/tháng',
'reason': lý do chi tiết cho các đề xuất trên, bao gồm các yếu tố thời tiết và nông học{{treatment_field}},
'predicted_disease': tên bệnh dự đoán, nếu không có bệnh thì là 'Không có',
LƯU Ý: Đây là dữ liệu cho hệ thống tự động tưới cây thông minh dùng ESP32, vì vậy trả lời phải CHÍNH XÁC theo định dạng JSON đã yêu cầu, không thêm bất kỳ ký tự nào khác.
)";

const char* PROMPT_TREATMENT_ANALYSIS = R"(
{{role_intro}}
{{env_info}}
{{disease_section}}

===== YÊU CẦU =====
{{requirements}}

Hãy định dạng câu trả lời dễ đọc, ngắn gọn trong khoảng 250 từ và sử dụng emoji phù hợp.
)";