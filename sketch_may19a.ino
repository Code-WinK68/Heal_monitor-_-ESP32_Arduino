#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>

// =======================
// OLED I2C BUS 0
// =======================
#define OLED_SDA_PIN 6
#define OLED_SCL_PIN 7

// =======================
// MLX90614 I2C BUS 1
// =======================
#define MLX_SDA_PIN 8
#define MLX_SCL_PIN 9

// =======================
// OLED SH1106 1.3 inch
// =======================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2( U8G2_R0,U8X8_PIN_NONE);

// OLED SSD1306 0.96 inch thì dùng dòng dưới thay cho dòng SH1106 bên trên:
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


// =======================
// MLX90614
// =======================
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

bool mlxOK = false;
float ambientTemp = 0.0;        //Nhiệt độ môi trường
float objectTemp = 0.0;         //Nhiệt độ bề mặt cần đo

unsigned long lastReadMs = 0;   // Thời gian đọc cảm biến
unsigned long lastOledMs = 0;   // Thời gian cập nhật màn hình

// =======================
// OLED DISPLAY
// =======================
void drawOLED(const char *statusText) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  u8g2.drawStr(0, 10, "MLX90614 TEST");      // Tiêu đề3 
  u8g2.drawLine(0, 12, 127, 12);             // Vẽ đường thẳng ngang phân cách

  char line[32];

// In trạng thái hệ thống
  snprintf(line, sizeof(line), "Status: %s", statusText);
  u8g2.drawStr(0, 25, line);

// In nhiệt độ môi trường 
  snprintf(line, sizeof(line), "Ambient: %.2f C", ambientTemp);
  u8g2.drawStr(0, 38, line);

// In nhiệt độ bề mặt đo 
  snprintf(line, sizeof(line), "Object : %.2f C", objectTemp);
  u8g2.drawStr(0, 51, line);

  u8g2.drawStr(0, 63, "MLX: SDA8 SCL9");

// Đẩy dữ liệu từ RAM lên màn hình
  u8g2.sendBuffer();    
}

// =======================
// SCAN I2C BUS MLX
// =======================
void scanMLXBus() {
  Serial.println("Dang scan I2C bus MLX90614...");

  int count = 0;

  for (byte address = 1; address < 127; address++) {
    Wire1.beginTransmission(address);
    byte error = Wire1.endTransmission();

    if (error == 0) {
      Serial.print("Tim thay I2C device tai dia chi 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);   // In địa chỉ dạng Hex MLX90614 :0x5A)
      count++;
    }
  }

  if (count == 0) {
    Serial.println("Khong tim thay thiet bi I2C nao tren Wire1");
  }

  Serial.println();
}

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("====================================");
  Serial.println("      MLX90614 + OLED TEST");
  Serial.println("====================================");

  // OLED dung Wire mac dinh
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);    // Gán chân 6,7 cho Wire
  Wire.setClock(400000);                     // Chạy tốc độ cao 400kHz cho Oled mượt 

  u8g2.begin();
  u8g2.setContrast(220);                    // Tăng độ sáng cho màn hình Oled
  drawOLED("BOOTING");                      // Hiển thị chữ này trong khih đợi nạp phần cảm biến

  // MLX90614 dung Wire1 rieng
  Wire1.begin(MLX_SDA_PIN, MLX_SCL_PIN);    // Gán chân 8,9 cho Wire1 riêng biệt
  Wire1.setClock(100000);                   // Chạy tốc độ tiêu chuẩn 100kHz cho cảm biến ổn định    
  Wire1.setTimeOut(50);                    // Tăng Timeout tránh treo chip do Clock Stretching của MLX

  scanMLXBus();     // Quét thiết bị

  Serial.println("Đang khởi tạo MLX90614...");

// Gọi hàm begin của thư viện, ép nó sử dụng địa chỉ 0*5A trên kênh Wire1
  if (!mlx.begin(0x5A, &Wire1)) {
    mlxOK = false;
    Serial.println("LOI: Khong tim thay MLX90614");
    Serial.println("Kiem tra day noi:");
    Serial.println("MLX90614 VCC -> 3.3V");
    Serial.println("MLX90614 GND -> GND");
    Serial.println("MLX90614 SDA -> GPIO8");
    Serial.println("MLX90614 SCL -> GPIO9");
    Serial.println("Dia chi MLX90614 thuong la 0x5A");

    drawOLED("MLX FAIL");

    while (1) {
      delay(1000);
    }
  }

  mlxOK = true;

  Serial.println("MLX90614 INIT OK");
  Serial.println("Bat dau doc nhiet do...");
  Serial.println();

  drawOLED("MLX OK");
}

// =======================
// LOOP
// =======================
void loop() {
  if (!mlxOK) return;                 // Nếu cảm biến lỗi từ lúc setup, bỏ qua không làm gì cả 

  // Đọc dữ liệu cảm biến  (500ms)
  if (millis() - lastReadMs >= 500) {
    lastReadMs = millis();

    ambientTemp = mlx.readAmbientTempC();
    objectTemp = mlx.readObjectTempC();

    Serial.print("Ambient = ");
    Serial.print(ambientTemp, 2);
    Serial.print(" C");

    Serial.print(" | Object = ");
    Serial.print(objectTemp, 2);
    Serial.println(" C");
  }

  // Cập màn hình Oled (500ms)
  if (millis() - lastOledMs >= 500) {
    lastOledMs = millis();

    // Check xem dữ liệu đọc về bị lỗi không toán học không
    if (isnan(ambientTemp) || isnan(objectTemp)) {
      drawOLED("READ ERROR");       // Báo lỗi nếu dây lỏng/nhiễu 
    } else {
      drawOLED("DANG DO");          // Hiển thị trạng thái chạy bình thường 
    }
  }
}