
#include <Wire.h>
#include <U8g2lib.h>
#include "MAX30105.h"


// =======================
// OLED I2C BUS 0 --> Dùng chung cho Oled & MAX30102
// =======================
#define BUS0_SDA_PIN 6
#define BUS0_SCL_PIN 7

// =======================
// OLED SH1106 1.3 inch
// =======================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2( U8G2_R0,U8X8_PIN_NONE);


// =======================
// MAX30102
// =======================
MAX30105 particleSensor;  

bool maxOK = false;

long irValue = 0;        // Giá trị hồng ngoại thô từ MAX30102 
long redValue = 0;       // Giá trị ánh sáng đỏ thô từ MAX30102 

unsigned long lastReadMs = 0;
unsigned long lastOledMs = 0;

// =======================
// OLED DISPLAY
// =======================
void drawOLED(const char *statusText) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(0, 10, "MAX30102 TEST");      // Tiêu đề3 
  u8g2.drawLine(0, 12, 127, 12);             // Vẽ đường thẳng ngang phân cách

  char line[32];

  // In trạng thái hệ thống
  snprintf(line, sizeof(line), "Status: %s", statusText);
  u8g2.drawStr(0, 23, line);


  snprintf(line, sizeof(line), "IR Raw : %ld", irValue);
  u8g2.drawStr(0, 34, line);

  snprintf(line, sizeof(line), "RED Raw: %ld", redValue);
  u8g2.drawStr(0, 45, line);

 // Check xem có đặt tay vào không
 if(irValue < 50000){
  u8g2.drawStr(0,55,"Finger : No Finger");
 }else {
  u8g2.drawStr(0,55, "Finger : Detected");
 }

  u8g2.drawStr(0, 64, "MAX: SDA6 SCL7");

// Đẩy dữ liệu từ RAM lên màn hình
  u8g2.sendBuffer();    
}


// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("====================================");
  Serial.println("      MAX30102 + OLED TEST");
  Serial.println("====================================");

  // OLED dung Wire mac dinh
  Wire.begin(BUS0_SDA_PIN, BUS0_SCL_PIN);    // Gán chân 6,7 cho Wire
  Wire.setClock(400000);                     // Chạy tốc độ cao 400kHz cho Oled mượt 

  u8g2.begin();
  u8g2.setContrast(220);                    // Tăng độ sáng cho màn hình Oled
  drawOLED("BOOTING");                      // Hiển thị chữ này trong khih đợi nạp phần cảm biến



  Serial.println("Đang khởi tạo MAX30102...");

  // Gọi hàm begin của thư viện, ép nó sử dụng địa chỉ 0*5A trên kênh Wire1
  if (!particleSensor.begin(Wire, 400000)) {
    maxOK = false;

    Serial.println("LOI: Khong tim thay MAX30102");
    Serial.println("Kiem tra day noi:");
    Serial.println("MAX30102 VCC -> 3.3V");
    Serial.println("MAX30102 GND -> GND");
    Serial.println("MAX30102 SDA -> GPIO6");
    Serial.println("MAX30102 SCL -> GPIO7");
    Serial.println("Dia chi MAX30102 thuong la 0x57");

    drawOLED("MAX FAIL");

    while (1) {
      delay(1000);
    }
  }
  // Cấu hình bật chế độ đo cả 2 đèn đỏ và hồng ngoại 
  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  maxOK = true;
  

  Serial.println("MAX30102 INIT OK");
  Serial.println("Bat dau doc du lieu...");
  Serial.println();

  drawOLED("MAX OK");
}


// =======================
// LOOP
// =======================
void loop() {
  if (!maxOK) return;                 // Nếu cảm biến lỗi từ lúc setup, bỏ qua không làm gì cả 

  // Đọc dữ liệu cảm biến  (500ms)
  if (millis() - lastReadMs >= 500) {
    lastReadMs = millis();

    irValue = particleSensor.getIR();
    redValue = particleSensor.getRed();

    Serial.print("IR Raw =");
    Serial.print(irValue);
    Serial.print(",");  
    Serial.print("Red Raw = ");
    Serial.println(redValue); 
  }

  // Cập màn hình Oled (500ms)
  if (millis() - lastOledMs >= 500) {
    lastOledMs = millis();

    // Check xem dữ liệu đọc về bị lỗi không toán học không
    if (irValue == 0 || redValue == 0) {
      drawOLED("READ ERROR");       // Báo lỗi nếu dây lỏng/nhiễu 
    } else {
      drawOLED("Dang do...");        // Hiển thị trạng thái chạy bình thường 
    }
  }
}

