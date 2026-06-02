# Hệ Thống Đánh Giá Chất Lượng Tim Mạch & Giám Sát Sức Khỏe (ESP32)
### Đồ án kỹ thuật - Nhóm 10 (Mã số: 169226)

Hệ thống nhúng thu thập, lọc số (Digital Signal Processing) và hiển thị thời gian thực các thông số sinh học bao gồm: Điện tâm đồ (ECG), Nồng độ oxy trong máu (SpO2), Nhịp tim (BPM) và Nhiệt độ cơ thể không tiếp xúc. Hệ thống sử dụng vi điều khiển **ESP32**, phân tầng giao diện bằng Menu điều hướng qua nút nhấn và màn hình OLED SH1106.

---

## Tính Năng Nổi Bật

* **Kiến trúc Dual Hardware I2C độc lập:** Tách biệt ngoại vi hiển thị (OLED) và cảm biến nhạy cảm (MLX90614) trên 2 kênh phần cứng riêng biệt nhằm chống nghẽn và treo chip do hiện tượng *Clock Stretching*.
* **Xử lý tín hiệu số ECG thời gian thực:** Tích hợp bộ lọc đáp ứng xung vô hạn (IIR/EMA) tần số lấy mẫu $250\text{ Hz}$ để khử nhiễu đường nền (Baseline Wander) và làm mịn tín hiệu (Smoothing).
* **Đồ thị động tự động co giãn (Auto-scaling):** Tự động tính toán biên độ đỉnh-đỉnh của tín hiệu ECG để hiển thị trọn vẹn dạng sóng trên màn hình OLED 128x64.
* **Giao diện đa nhiệm FSM (Finite State Machine):** Quản lý luồng màn hình thông minh (Home Dashboard, Mạng WiFi, Cấu hình Thẻ nhớ SD, Xem chi tiết cảm biến) thông qua cụm 4 nút nhấn chống rung (Debounce) thuật toán.
* **Cơ chế bẫy lỗi thông minh:** Tự động phát hiện tuột dây điện cực ECG (Leads Off), tín hiệu bão hòa (Saturated), hoặc chưa đặt ngón tay vào cảm biến SpO2 (Finger Detection) để bảo vệ tính toàn vẹn của dữ liệu.

---

## 🛠️ Thành Phần Phần Cứng

1. **Vi điều khiển:** ESP32 DevKit Board.
2. **Cảm biến ECG:** AD8232 (Single Lead Heart Rate Monitor).
3. **Cảm biến SpO2 & Nhịp tim:** MAX30102 (Sử dụng thư viện `MAX30105`).
4. **Cảm biến nhiệt độ:** MLX90614 (Hồng ngoại không tiếp xúc).
5. **Hiển thị:** Màn hình OLED SH1106 1.3 inch (I2C).
6. **Điều khiển:** Cụm 4 nút nhấn (UP, DOWN, OK, BACK).

---

## Sơ Đồ Kết Nối Ngoại Vi (Pin Mapping)

### 1. Kênh Giao Tiếp I2C & Hiển Thị
| Ngoại vi | Chân Linh Kiện | Chân ESP32 (GPIO) | Cấu hình & Vai trò |
| :--- | :--- | :--- | :--- |
| **OLED SH1106** | SDA | **GPIO 6** (Wire Bus 0) | Tốc độ cao $400\text{ kHz}$ |
| **OLED SH1106** | SCL | **GPIO 7** (Wire Bus 0) | Tốc độ cao $400\text{ kHz}$ |
| **MAX30102** | SDA | **GPIO 6** (Wire Bus 0) | Chung đường bus với OLED (`0x57`) |
| **MAX30102** | SCL | **GPIO 7** (Wire Bus 0) | Chung đường bus với OLED (`0x57`) |
| **MLX90614** | SDA | **GPIO 8** (Wire Bus 1) | Kênh độc lập, Tốc độ $100\text{ kHz}$ |
| **MLX90614** | SCL | **GPIO 9** (Wire Bus 1) | Timeout 50ms chống treo chip (`0x5A`) |

### 2. Cảm Biến Điện Tâm Đồ AD8232 (ECG)
| Chân AD8232 | Chân ESP32 (GPIO) | Chức năng |
| :--- | :--- | :--- |
| **OUTPUT** | **GPIO 1** | Ngõ ra tín hiệu tương tự (Analog Input - ADC 12-bit) |
| **LO+** | **GPIO 3** | Phát hiện tuột dây điện cực Dương (INPUT_PULLDOWN) |
| **LO-** | **GPIO 2** | Phát hiện tuột dây điện cực Âm (INPUT_PULLDOWN) |

### 3. Cụm Nút Nhấn Điều Hướng (UI Buttons)
| Nút Nhấn | Chân ESP32 (GPIO) | Cấu hình Logic |
| :--- | :--- | :--- |
| **BUTTON UP** | **GPIO 44** | INPUT_PULLUP (Kích hoạt mức THẤP) |
| **BUTTON DOWN** | **GPIO 4** | INPUT_PULLUP (Kích hoạt mức THẤP) |
| **BUTTON OK** | **GPIO 5** | INPUT_PULLUP (Kích hoạt mức THẤP) |
| **BUTTON BACK**| **GPIO 43** | INPUT_PULLUP (Kích hoạt mức THẤP) |

---

## Phân Tích Thuật Toán & Xử Lý Tín Hiệu

### 1. Kiến Trúc Lọc Số Bộ Đôi EMA (Exponential Moving Average) Cho ECG
Tín hiệu từ cảm biến AD8232 thường bị ảnh hưởng nghiêm trọng bởi nhiễu trôi dạt đường nền (do nhịp thở của người đo) và nhiễu cơ học tần số cao (do rung cơ). Hệ thống giải quyết bằng thuật toán lọc số hai giai đoạn:

* **Giai đoạn 1: Lọc Thông Cao (High-pass Filter) khử trôi đường nền:**
  Hệ thống tính toán giá trị trung bình DC động (`baseline`) rất chậm với hệ số $\alpha = 0.995$:
  $$\text{baseline} = 0.995 \times \text{baseline} + 0.005 \times \text{rawValue}$$
  Tín hiệu sạch thành phần xoay chiều (AC) được trích xuất bằng cách lấy: $\text{highPass} = \text{rawValue} - \text{baseline}$.
* **Giai đoạn 2: Lọc Thông Thấp (Low-pass Filter) làm mịn:**
  Khử các gai nhiễu răng cưa cao tần bằng hệ số mịn $\alpha = 0.18$:
  $$\text{filtered} = \text{filtered} + 0.18 \times (\text{highPass} - \text{filtered})$$



### 2. Cấu Hình Tối Ưu Cho MAX30102 (PPG)
Thanh ghi cảm biến quang học được cấu hình thông qua tập lệnh:
`particleSensor.setup(60, 4, 2, 100, 411, 4096);`
* **Sample Average (4):** Tự động lấy trung bình mỗi 4 mẫu phần cứng để triệt tiêu nhiễu gai xung trước khi gửi về ESP32.
* **Dual-LED Mode (2):** Kích hoạt đồng thời cả LED Đỏ (Red) và LED Hồng ngoại (IR) để phục vụ cho thuật toán so sánh tỷ lệ hấp thụ hemoglobin, phục vụ đo chỉ số bão hòa oxy SpO2.
* **Finger Detection Threshold:** Bẫy điều kiện `irValue < 50000` để nhận biết trạng thái không tiếp xúc của ngón tay một cách chính xác, tránh tính toán sai lệch dữ liệu.

---

## Cấu Trúc Mã Nguồn

Dự án được phát triển theo từng giai đoạn module hóa:
* `UI_Menu_Frame.ino`: Khung xương điều khiển màn hình và thuật toán FSM nút nhấn.
* `ECG_AD8232_DSP.ino`: Code lõi lấy mẫu $250\text{ Hz}$, tính toán bộ lọc EMA và vẽ đồ thị ECG lên OLED.
* `MLX90614_Dual_I2C.ino`: Cấu hình bus `Wire1` riêng biệt và đọc nhiệt độ hồng ngoại không tiếp xúc.
* `MAX30102_PPG_Oled.ino`: Cấu hình thanh ghi đọc dữ liệu thô Red/IR và thuật toán nhận diện ngón tay.

---

## Hướng Dẫn Cài Đặt & Sử Dụng

1. Tải và cài đặt các thư viện bắt buộc trong **Arduino IDE Library Manager**:
   * `U8g2` (bởi oliver)
   * `Adafruit MLX90614 Library`
   * `SparkFun MAX3010x Pulse and Proximity Sensor Library`
2. Kết nối phần cứng chính xác theo bảng **Pin Mapping** phía trên.
3. Mở mã nguồn tích hợp bằng Arduino IDE, cấu hình bo mạch là **ESP32 Dev Module** (hoặc loại kit tương đương bạn đang dùng).
4. Thiết lập độ phân giải ADC 12-bit (`analogReadResolution(12)`) và dải đo `ADC_11db` (tương đương tối đa ~3.3V).
5. Nạp chương trình. Màn hình BOOT ban đầu của **Nhóm 10** sẽ hiển thị trong 3 giây trước khi chuyển vào Dashboard chính.
