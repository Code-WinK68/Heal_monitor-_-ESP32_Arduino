#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>

// =====================================================
// PIN CONFIG
// =====================================================

// OLED I2C
#define OLED_SDA_PIN 6
#define OLED_SCL_PIN 7

// AD8232
#define LO_PLUS_PIN   3
#define OUTPUT_PIN    1
#define LO_MINUS_PIN  2

// =====================================================
// OLED CONFIG
// =====================================================
// OLED 1.3 inch SH1106
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE);

// Nếu OLED của bạn là SSD1306 0.96 inch thì comment dòng trên,
// và mở dòng dưới:
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =====================================================
// ECG CONFIG
// =====================================================
const uint32_t SAMPLE_RATE_HZ = 250;                            // Tần số lấy mẫu (250 mẫu/s)
const uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_RATE_HZ;   // Chu kỳ lấy mẫu (4ms)

unsigned long lastSampleUs = 0;         // Lưu mốc thời gian (us) lần cuối đọc ECG
unsigned long lastOledMs = 0;           // Lưu mốc thời gian (ms) lần cuối vẽ OLED
unsigned long lastSerialMs = 0;         // Lưu mốc thời gian (ms) lần cuối in Serial

int rawValue = 0;                       // Biến lưu giá trị ADC thô đọc từ cảm biến (0 - 4950)
int mvValue = 0;                        // Biến lưu giá trị điẹn áp quy đổi ra mV

bool leadsOff = true;                   // Trạng thái có bị tuột dây điệc cực hay không
bool saturated = false;                 // Trạng thái tín hiệu bị bão hòa (quá cao hay quá thấp)

// Các biến áp dụng cho thuật toán lọc số (DSP)
float baseline = 0.0f;                  // Giá trị đường nền (trung bình tín hiệu)
float filtered = 0.0f;                  // Giá trị sau khi đã lọc mượt 
bool filterReady = false;               // Cờ báo hiệu bộ lọc đã sẵn sàng hoạt động hay chưa

const float BASELINE_ALPHA = 0.995f;    // Hệ số lọc đường nền (lọc thông thấp rất chậm)
const float SMOOTH_ALPHA   = 0.18f;     // Hệ số làm mịn tín hiệu (lọc thông thấp khử răng cưa)

// =====================================================
// WAVE BUFFER ( bộ đệm sóng)
// =====================================================
#define WAVE_POINTS 128                // Số điểm trên đồ thị màn hình
 
int waveBuffer[WAVE_POINTS];           // Mảng lưu trữ các giá trị để vẽ đồ thị 
int waveIndex = 0;                     // Vị trí hiện tại đang ghi dữ liệu trong mảng
bool waveFilled = false;               // Cờ báo hiệu mảng đã được ghi đầy dữ liệu lần đầu tiên chưa


// =====================================================
// LEADS STABLE FILTER  (kiểm tra tuột dây)
// =====================================================
bool readLeadsOffStable() {
  static int offCount = 0;              // Bộ đếm số lần phát hiện bị tuột dây liên tiếp  
  static int okCount = 0;               // Bộ đếm số lần phát hiện kết nối tốt liên tiếp
  static bool stableLeadsOff = true;    // Trạng thái tuột dây sau khi đã lọc nhiễu


// Đọc trạng thái logic từ 2 chân LO+ và LO-
  bool loPlus = digitalRead(LO_PLUS_PIN);
  bool loMinus = digitalRead(LO_MINUS_PIN);
  bool nowOff = loPlus || loMinus;           // Nếu 1 trong 2 chân = 1 ==> tuột 

  if (nowOff) {
    offCount++;        // Tăng bộ đếm tuột dây
    okCount = 0;       // Reset bộ đếm ổn định
  } else {
    okCount++;         // Tăng bộ đếm ổn định
    offCount = 0;      // Reset bộ đệm tuột dây
  }

  // Phải OFF liên tục nhiều mẫu mới báo OFF
  if (offCount >= 10) {     
    stableLeadsOff = true;
  }

  // Phải OK liên tục nhiều mẫu mới báo OK
  if (okCount >= 30) {
    stableLeadsOff = false;
  }

  return stableLeadsOff;
}

// =====================================================
// WAVE FUNCTIONS
// =====================================================
// Hàm thêm 1 giá trị mới vào mảng để vẽ đồ thị 
void pushWave(int value) {
  waveBuffer[waveIndex] = value;     // Ghi giá trị vào vị trí hiện tại 
  waveIndex++;                       // Tăng vị trí cho lần sau

  if (waveIndex >= WAVE_POINTS) {    // Nếu quá 128 điểm
    waveIndex = 0;                   // Quay về vị trí đầu tiên 0 để ghi đè dữ liệu cữ
    waveFilled = true;               // Dánh dấu là mảng đã đầy
  }
}

// Hàm xóa toàn bộ dữ liệu đồ thị về 0
void clearWave() {
  for (int i = 0; i < WAVE_POINTS; i++) {
    waveBuffer[i] = 0;
  }

  waveIndex = 0;
  waveFilled = false;
}

// Hàm reset bộ lọc về trạng thái ban đầu
void resetFilter() {
  filterReady = false;
  baseline = rawValue;          // Gán đường nền = đúng giá trị thô hiện tại 
  filtered = 0.0f;
}

// =====================================================
// OLED DRAW
// =====================================================
void drawECGScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(0, 9, "AD8232 ECG TEST");
  u8g2.drawLine(0, 11, 127, 11);

  char line[32];

  if (leadsOff) {
    snprintf(line, sizeof(line), "Status: LEADS OFF");
  } else if (saturated) {
    snprintf(line, sizeof(line), "Status: SATURATED");
  } else {
    snprintf(line, sizeof(line), "Status: LEADS OK");
  }

  u8g2.drawStr(0, 21, line);

  snprintf(line, sizeof(line), "RAW:%d  %dmV", rawValue, mvValue);
  u8g2.drawStr(0, 32, line);

  // Graph area
  const int graphX = 0;
  const int graphY = 35;
  const int graphW = 128;
  const int graphH = 28;

  u8g2.drawFrame(graphX, graphY, graphW, graphH);

  // Center line
  u8g2.drawLine(
    graphX + 1,
    graphY + graphH / 2,
    graphX + graphW - 2,
    graphY + graphH / 2
  );

  int count = waveFilled ? WAVE_POINTS : waveIndex;

  if (count > 2) {
    int minVal = waveBuffer[0];
    int maxVal = waveBuffer[0];

    for (int i = 1; i < count; i++) {
      if (waveBuffer[i] < minVal) minVal = waveBuffer[i];
      if (waveBuffer[i] > maxVal) maxVal = waveBuffer[i];
    }

    if (maxVal - minVal < 20) {
      maxVal += 10;
      minVal -= 10;
    }

    int start = waveFilled ? waveIndex : 0;

    int lastX = graphX + 1;
    int lastY = graphY + graphH / 2;

    for (int i = 0; i < count; i++) {
      int idx = (start + i) % WAVE_POINTS;
      int value = waveBuffer[idx];

      int x = map(i, 0, WAVE_POINTS - 1, graphX + 1, graphX + graphW - 2);
      int y = map(value, minVal, maxVal, graphY + graphH - 2, graphY + 1);

      if (y < graphY + 1) y = graphY + 1;
      if (y > graphY + graphH - 2) y = graphY + graphH - 2;

      if (i > 0) {
        u8g2.drawLine(lastX, lastY, x, y);
      }

      lastX = x;
      lastY = y;
    }
  }

  u8g2.sendBuffer();
}

// =====================================================
// ECG READ
// =====================================================
void readECG() {
  // Kiểm tra xem đã đến lúc lấy mẫu chưa (phải đủ 4ms mới chạy tiếp)
  if ((uint32_t)(micros() - lastSampleUs) < SAMPLE_PERIOD_US) return ;
  lastSampleUs += SAMPLE_PERIOD_US;  // Cập nhật mốc thời gian lấy mẫu tiếp theo 

  rawValue = analogRead(OUTPUT_PIN); // Đọc giá trị điện áp thô từ cảm biến (0 - 4950)

  leadsOff = readLeadsOffStable();   // Kiểm tra tuột dây

  // Nếu OUT kẹt quá cao hoặc quá thấp thì coi như bão hòa
  saturated = (rawValue < 50 || rawValue > 4040);

  // Nếu tuột dây hoặc bão hòa , reset bộ lọc, đẩy giá trị 0 vào đồ thị rồi thoát hàm
  if (leadsOff || saturated) {
    resetFilter();
    pushWave(0);
    return;
  }
  // Nếu bộ lọc chưa sẵn sàng (vừa cắm dây lại), khởi tạo giá trị cho bộ lọc 
  if (!filterReady) {
    baseline = rawValue;
    filtered = 0.0f;
    filterReady = true;
  }

  // Lọc baseline
  baseline = BASELINE_ALPHA * baseline + (1.0f - BASELINE_ALPHA) * rawValue;

  // High-pass
  float highPass = rawValue - baseline;

  // Smooth
  filtered = filtered + SMOOTH_ALPHA * (highPass - filtered);

  // Tăng biên độ cho dễ nhìn trên OLED
  int displayValue = (int)roundf(filtered * 5.0f);

  pushWave(displayValue);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Dùng pulldown để LO+/LO- đỡ bị nhiễu khi tín hiệu chập chờn
  pinMode(LO_PLUS_PIN, INPUT_PULLDOWN);
  pinMode(LO_MINUS_PIN, INPUT_PULLDOWN);
  pinMode(OUTPUT_PIN, INPUT);

  analogReadResolution(12);                      // Cấu hình ADC độ phân giải 12bit
  analogSetPinAttenuation(OUTPUT_PIN, ADC_11db); // Cấu hình dải đo điện áp ADC tối đa khoảng 3.3V

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);        // Khởi tạo I2C
  Wire.setClock(400000);                         // Tăng tốc độ I2C lên 400kHz
 
  u8g2.begin();                        // Khởi tạo màn hình Oled
  u8g2.setContrast(220);               // Đặt độ sáng màn hình 

  clearWave();                         // Xóa trống mảng đồ thị ban đầu 
  lastSampleUs = micros();             // Ghi nhận mốc thời gian bắt đầu chạy 

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 12, "AD8232 ECG TEST");
  u8g2.drawStr(0, 28, "OLED OK");
  u8g2.drawStr(0, 44, "Khoi dong...");
  u8g2.sendBuffer();

  delay(1000);
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  readECG();

  if (millis() - lastOledMs >= 40) {
    lastOledMs = millis();

    mvValue = analogReadMilliVolts(OUTPUT_PIN);
    drawECGScreen();
  }
/*
 if (millis() - lastSerialMs >= 200) {
    lastSerialMs = millis();

    Serial.print("raw:");
    Serial.print(rawValue);

    Serial.print("\tmV:");
    Serial.print(mvValue);

    Serial.print("\tLO_PLUS:");
    Serial.print(digitalRead(LO_PLUS_PIN));

    Serial.print("\tLO_MINUS:");
    Serial.print(digitalRead(LO_MINUS_PIN));

    Serial.print("\t");

    if (leadsOff) {
      Serial.println("LEADS_OFF");
    } else if (saturated) {
      Serial.println("SATURATED");
    } else {
      Serial.println("LEADS_OK");
    }
  }
  */
  
  if (millis() - lastSerialMs >= 400) { 
    lastSerialMs = millis();
    Serial.print("Raw_ECG:");
    Serial.println(rawValue); 
  }

}