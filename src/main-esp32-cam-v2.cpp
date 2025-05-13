#include <plant-disease-system_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"
#include <TFT_eSPI.h>

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static bool is_initialised = false;
uint8_t *snapshot_buf;

// Khởi tạo đối tượng TFT
TFT_eSPI tft = TFT_eSPI();

// Định nghĩa màu UI
#define UI_BACKGROUND     TFT_BLACK
#define UI_HEADER_BG      0x4228  // Màu xanh đậm cho header
#define UI_HEADER_TEXT    TFT_WHITE
#define UI_ACCENT         0x5E5C  // Màu xanh dương cho accent
#define UI_TEXT           TFT_WHITE
#define UI_TEXT_SECONDARY 0xBDF7  // Màu xám nhạt
#define UI_SUCCESS        0x07E0  // Xanh lá
#define UI_WARNING        0xFD20  // Cam vàng
#define UI_DANGER         0xF800  // Đỏ
#define UI_INFO           0x07FF  // Xanh dương nhạt
#define UI_BORDER         0x8430  // Màu viền xám

// Định nghĩa font
#define FONT_SMALL        1
#define FONT_NORMAL       2
#define FONT_LARGE        4

// Ngưỡng để hiển thị kết quả (chỉ hiển thị trên 50%)
#define CONFIDENCE_THRESHOLD 0.5f

// Định nghĩa kích thước và vị trí các thành phần UI
#define HEADER_HEIGHT     30
#define RESULT_START_Y    90
#define BORDER_RADIUS     4
#define BAR_HEIGHT        10
#define BAR_MAX_WIDTH     150
#define RESULT_ITEM_HEIGHT 40
#define PADDING           8

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);

// Vẽ hình chữ nhật bo tròn góc
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.drawRoundRect(x, y, w, h, r, color);
}

// Vẽ hình chữ nhật bo tròn góc đã tô màu
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, r, color);
}

// Vẽ thanh tiến trình
void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t progress, uint16_t barColor, uint16_t bgColor) {
  fillRoundRect(x, y, w, h, h/2, bgColor);
  if (progress > 0) {
    int16_t progressWidth = (progress * w) / 100;
    if (progressWidth < h/2) progressWidth = h/2;
    fillRoundRect(x, y, progressWidth, h, h/2, barColor);
  }
}

// Khởi tạo giao diện
void initializeUI() {
  // Xóa toàn bộ màn hình
  tft.fillScreen(UI_BACKGROUND);
  
  // Vẽ Header
  fillRoundRect(0, 0, tft.width(), HEADER_HEIGHT, 0, UI_HEADER_BG);
  tft.setTextColor(UI_HEADER_TEXT);
  tft.setTextDatum(MC_DATUM); // Canh giữa
  tft.setTextSize(1);
  tft.drawString("PLANT DISEASE DETECTION", tft.width()/2, HEADER_HEIGHT/2, FONT_NORMAL);
  
  // Vẽ viền cho khu vực kết quả
  drawRoundRect(PADDING, RESULT_START_Y - PADDING, 
                tft.width() - PADDING*2, tft.height() - RESULT_START_Y, 
                BORDER_RADIUS, UI_BORDER);
  
  // Khu vực trạng thái camera
  fillRoundRect(PADDING, HEADER_HEIGHT + PADDING, tft.width() - PADDING*2, 40, BORDER_RADIUS, 0x2104);
}

// Hiển thị trạng thái camera
void updateCameraStatus(bool success) {
  tft.fillRoundRect(PADDING + 5, HEADER_HEIGHT + PADDING + 5, tft.width() - PADDING*2 - 10, 30, BORDER_RADIUS, UI_BACKGROUND);
  
  tft.setTextDatum(ML_DATUM); // Middle-Left
  tft.setTextSize(1);
  
  if (success) {
    tft.setTextColor(UI_SUCCESS);
    tft.drawString("Camera: ", PADDING + 10, HEADER_HEIGHT + PADDING + 20, FONT_NORMAL);
    tft.setTextColor(UI_TEXT);
    tft.drawString("Connected", PADDING + 80, HEADER_HEIGHT + PADDING + 20, FONT_NORMAL);
    
    // Vẽ biểu tượng camera
    tft.fillCircle(tft.width() - PADDING - 20, HEADER_HEIGHT + PADDING + 20, 8, UI_SUCCESS);
  } else {
    tft.setTextColor(UI_DANGER);
    tft.drawString("Camera: ", PADDING + 10, HEADER_HEIGHT + PADDING + 20, FONT_NORMAL);
    tft.setTextColor(UI_TEXT);
    tft.drawString("Failed to initialize", PADDING + 80, HEADER_HEIGHT + PADDING + 20, FONT_NORMAL);
    
    // Vẽ biểu tượng lỗi
    tft.fillCircle(tft.width() - PADDING - 20, HEADER_HEIGHT + PADDING + 20, 8, UI_DANGER);
  }
}

// Hiển thị tiêu đề khu vực kết quả
void displayResultsHeader() {
  tft.fillRect(PADDING, RESULT_START_Y, tft.width() - PADDING*2, 30, UI_BACKGROUND);
  
  tft.setTextColor(UI_ACCENT);
  tft.setTextDatum(ML_DATUM); // Middle-Left
  tft.setTextSize(1);
  tft.drawString("DETECTION RESULTS (>50%)", PADDING + 10, RESULT_START_Y + 15, FONT_NORMAL);
  
  // Vẽ đường kẻ ngang phân tách
  tft.drawLine(PADDING, RESULT_START_Y + 30, tft.width() - PADDING, RESULT_START_Y + 30, UI_BORDER);
}

// Hiển thị thời gian xử lý
void displayProcessingTime(int dsp_time, int classification_time) {
  int y_pos = HEADER_HEIGHT + PADDING + 50;
  
  // Xóa khu vực hiển thị thời gian
  tft.fillRect(PADDING, y_pos, tft.width() - PADDING*2, 30, UI_BACKGROUND);
  
  // Hiển thị thông tin thời gian
  tft.setTextDatum(ML_DATUM);
  tft.setTextSize(1);
  
  tft.setTextColor(UI_TEXT_SECONDARY);
  tft.drawString("Processing time:", PADDING + 10, y_pos + 10, FONT_SMALL);
  
  char timing_str[64];
  sprintf(timing_str, "DSP: %dms | Classification: %dms", dsp_time, classification_time);
  tft.setTextColor(UI_TEXT);
  tft.drawString(timing_str, PADDING + 10, y_pos + 25, FONT_SMALL);
}

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        // Swap BGR to RGB here
        // due to https://github.com/espressif/esp32-camera/issues/379
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}

void setup(){
    Serial.begin(115200);
    while (!Serial);
    Serial.println("Edge Impulse Inferencing Demo");
    
    // Khởi tạo màn hình TFT
    tft.init();
    tft.setRotation(3); // Landscape mode
    
    // Khởi tạo giao diện
    initializeUI();
    
    if (ei_camera_init() == false) {
        ei_printf("Failed to initialize Camera!\r\n");
        updateCameraStatus(false);
    }
    else {
        ei_printf("Camera initialized\r\n");
        updateCameraStatus(true);
    }

    // Hiển thị thông báo đang khởi động
    tft.setTextColor(UI_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Starting inference...", tft.width()/2, RESULT_START_Y - 25, FONT_NORMAL);
    
    // Hiển thị tiêu đề khu vực kết quả
    displayResultsHeader();
    
    ei_printf("\nStarting continious inference in 2 seconds...\n");
    ei_sleep(2000);
}

void loop(){

    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        return;
    }

    // print the predictions
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);
    ei_printf("Predictions:\r\n");
    
    // Hiển thị thời gian xử lý
    displayProcessingTime(result.timing.dsp, result.timing.classification);
    
    // Xóa khu vực hiển thị kết quả (giữ viền)
    tft.fillRect(PADDING + 1, RESULT_START_Y + 31, 
                tft.width() - PADDING*2 - 2, tft.height() - RESULT_START_Y - PADDING - 32, 
                UI_BACKGROUND);
    
    bool found_above_threshold = false;
    int y_pos = RESULT_START_Y + 40;
    
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf("%.5f\r\n", result.classification[i].value);
        
        // Chỉ hiển thị các dự đoán có giá trị > 50% lên màn hình LCD
        if (result.classification[i].value > CONFIDENCE_THRESHOLD) {
            found_above_threshold = true;
            float confidence = result.classification[i].value;
            
            uint16_t bar_color;
            // Chọn màu dựa trên độ tin cậy
            if (confidence > 0.8f) {
                bar_color = UI_DANGER; // Nguy cơ rất cao - đỏ
            } else if (confidence > 0.65f) {
                bar_color = UI_WARNING; // Nguy cơ cao - vàng cam
            } else {
                bar_color = UI_INFO; // Nguy cơ trung bình - xanh dương
            }
            
            // Vẽ nền nhạt cho mỗi hàng kết quả
            uint16_t rowBgColor = (i % 2 == 0) ? 0x1082 : UI_BACKGROUND;
            tft.fillRect(PADDING + 5, y_pos, tft.width() - PADDING*2 - 10, RESULT_ITEM_HEIGHT, rowBgColor);
            
            // Tên bệnh
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(UI_TEXT);
            tft.drawString(ei_classifier_inferencing_categories[i], PADDING + 15, y_pos + 12, FONT_NORMAL);
            
            // Phần trăm độ tin cậy
            char conf_str[10];
            sprintf(conf_str, "%.1f%%", confidence * 100.0f);
            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(confidence > 0.8f ? UI_DANGER : UI_TEXT);
            tft.drawString(conf_str, tft.width() - PADDING - 15, y_pos + 12, FONT_NORMAL);
            
            // Vẽ thanh tiến trình
            int bar_width = BAR_MAX_WIDTH;
            int progress_percent = confidence * 100;
            drawProgressBar(PADDING + 15, y_pos + 25, bar_width, BAR_HEIGHT, 
                           progress_percent, bar_color, 0x4208);
            
            // Di chuyển đến vị trí tiếp theo
            y_pos += RESULT_ITEM_HEIGHT + 5;
        }
    }
    
    // Hiển thị thông báo nếu không có kết quả nào > 50%
    if (!found_above_threshold) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(UI_SUCCESS);
        tft.drawString("No disease detected (>50%)", tft.width()/2, RESULT_START_Y + 80, FONT_NORMAL);
        
        // Vẽ biểu tượng tích
        tft.fillCircle(tft.width()/2, RESULT_START_Y + 115, 15, 0x1084);
        tft.setTextColor(UI_TEXT);
        tft.drawString("✓", tft.width()/2, RESULT_START_Y + 115, FONT_LARGE);
    }
    
    free(snapshot_buf);
}
