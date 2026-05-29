#include <Wire.h>
#include <U8g2lib.h>

// =====================================================
// PIN CONFIG
// =====================================================
#define SDA_PIN 6
#define SCL_PIN 7

// Buttons / Switch
#define BTN_UP_PIN    44
#define BTN_DOWN_PIN  4
#define BTN_OK_PIN    5
#define BTN_BACK_PIN  43

// =====================================================
// OLED CONFIG
// =====================================================
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =====================================================
// TIMING
// =====================================================
const uint32_t UI_REFRESH_MS = 80;
const uint32_t BUTTON_DEBOUNCE_MS = 25;

unsigned long lastUIRefreshMs = 0;

// =====================================================
// SENSOR / SYSTEM DATA
// Sau nay thay cac bien demo nay bang bien that tu code tich hop
// =====================================================

// ECG / AD8232
int ecgValue = 1234;
long ecgHeartRate = 75;
bool ecgOK = true;
bool ecgHeartRateValid = true;
bool leadsOff = false;

// MAX30102 / SpO2
long spo2 = 98;
bool maxOK = true;
bool validSPO2 = true;
bool maxFingerPresent = true;

// MLX90614
float bodyTemp = 36.5;
bool mlxOK = true;

// SD Card
bool sdOK = true;
bool loggingEnabled = true;
uint32_t sdFreeMB = 1024;

// WiFi
const char* WIFI_SSID = "ESP32-Health";
const char* WIFI_PASS = "12345678";
const char* WIFI_IP   = "192.168.4.1";

// =====================================================
// BUTTONS
// =====================================================
enum ButtonID {
  BTN_UP = 0,
  BTN_DOWN,
  BTN_OK,
  BTN_BACK,
  BTN_COUNT
};

struct ButtonState {
  uint8_t pin;
  bool stableState;
  bool lastReading;
  bool pressedEvent;
  unsigned long lastDebounceTime;
};

ButtonState buttons[BTN_COUNT] = {
  {BTN_UP_PIN,   HIGH, HIGH, false, 0},
  {BTN_DOWN_PIN, HIGH, HIGH, false, 0},
  {BTN_OK_PIN,   HIGH, HIGH, false, 0},
  {BTN_BACK_PIN, HIGH, HIGH, false, 0}
};

// =====================================================
// SCREEN STATE
// =====================================================
enum ScreenState {
  SCREEN_HOME = 0,

  SCREEN_MAIN_MENU,

  SCREEN_MONITOR_MENU,
  SCREEN_MONITOR_ECG,
  SCREEN_MONITOR_SPO2,
  SCREEN_MONITOR_TEMP,

  SCREEN_SD_MENU,
  SCREEN_SD_LOG_STATUS,
  SCREEN_SD_CHECK,

  SCREEN_WIFI_IP
};

ScreenState currentScreen = SCREEN_HOME;

int selectedMainMenu = 0;
int selectedMonitorMenu = 0;
int selectedSDMenu = 0;

// =====================================================
// MENU TEXT
// =====================================================
const char* const MAIN_MENU_ITEMS[] = {
  "1. Che Do Xem",
  "2. The Nho SD",
  "3. Mang WiFi"
};

const char* const MONITOR_MENU_ITEMS[] = {
  "1.1. Dien Tim ECG",
  "1.2. Chi So SpO2",
  "1.3. Nhiet Do MLX"
};

const char* const SD_MENU_ITEMS[] = {
  "2.1. Trang Thai Ghi",
  "2.2. Kiem Tra The"
};

const int MAIN_MENU_COUNT = sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);
const int MONITOR_MENU_COUNT = sizeof(MONITOR_MENU_ITEMS) / sizeof(MONITOR_MENU_ITEMS[0]);
const int SD_MENU_COUNT = sizeof(SD_MENU_ITEMS) / sizeof(SD_MENU_ITEMS[0]);

// =====================================================
// OLED HELPERS
// =====================================================
void drawHeader(const char* title) {
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 10, title);
  u8g2.drawLine(0, 12, 127, 12);
}

void drawFooter(const char* text) {
  u8g2.drawLine(0, 54, 127, 54);
  u8g2.drawStr(0, 64, text);
}

void drawInfoScreen(
  const char* title,
  const char* line1,
  const char* line2,
  const char* line3,
  const char* footer
) {
  u8g2.clearBuffer();
  drawHeader(title);

  if (line1 && line1[0] != '\0') u8g2.drawStr(0, 24, line1);
  if (line2 && line2[0] != '\0') u8g2.drawStr(0, 36, line2);
  if (line3 && line3[0] != '\0') u8g2.drawStr(0, 48, line3);

  drawFooter(footer);
  u8g2.sendBuffer();
}

void drawMenuPage(
  const char* title,
  const char* const items[],
  int itemCount,
  int selectedIndex
) {
  u8g2.clearBuffer();
  drawHeader(title);

  int y = 24;

  for (int i = 0; i < itemCount; i++) {
    if (i == selectedIndex) {
      u8g2.drawStr(0, y, ">");
      u8g2.drawBox(8, y - 8, 118, 10);
      u8g2.setDrawColor(0);
      u8g2.drawStr(10, y, items[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(10, y, items[i]);
    }

    y += 12;
  }

  drawFooter("UP/DN Chon OK Vao");
  u8g2.sendBuffer();
}

// =====================================================
// DRAW SCREENS
// =====================================================
void drawHomeScreen() {
  char line1[32];
  char line2[32];
  char line3[32];

  if (mlxOK) snprintf(line1, sizeof(line1), "Temp : %.1f C", bodyTemp);
  else snprintf(line1, sizeof(line1), "Temp : ERR");

  if (ecgOK && !leadsOff && ecgHeartRateValid) {
    snprintf(line2, sizeof(line2), "BPM  : %ld", ecgHeartRate);
  } else if (leadsOff) {
    snprintf(line2, sizeof(line2), "BPM  : No Lead");
  } else {
    snprintf(line2, sizeof(line2), "BPM  : ERR");
  }

  if (maxOK && maxFingerPresent && validSPO2) {
    snprintf(line3, sizeof(line3), "SpO2 : %ld %%", spo2);
  } else if (!maxFingerPresent) {
    snprintf(line3, sizeof(line3), "SpO2 : No Finger");
  } else {
    snprintf(line3, sizeof(line3), "SpO2 : ERR");
  }

  drawInfoScreen("HOME / DASHBOARD", line1, line2, line3, "OK Menu");
}

void drawECGScreen() {
  char line1[32];
  char line2[32];
  char line3[32];

  if (!ecgOK) {
    drawInfoScreen("1.1 DIEN TIM ECG", "ECG Sensor: ERR", "Kiem tra AD8232", "", "BACK Ve  OK Home");
    return;
  }

  if (leadsOff) {
    drawInfoScreen("1.1 DIEN TIM ECG", "Status : No Lead", "Gan lai dien cuc", "", "BACK Ve  OK Home");
    return;
  }

  snprintf(line1, sizeof(line1), "ECG Raw: %d", ecgValue);

  if (ecgHeartRateValid) snprintf(line2, sizeof(line2), "BPM    : %ld", ecgHeartRate);
  else snprintf(line2, sizeof(line2), "BPM    : ERR");

  snprintf(line3, sizeof(line3), "Lead   : OK");

  drawInfoScreen("1.1 DIEN TIM ECG", line1, line2, line3, "BACK Ve  OK Home");
}

void drawSpO2Screen() {
  char line1[32];

  if (!maxOK) {
    drawInfoScreen("1.2 CHI SO SPO2", "MAX30102: ERR", "Kiem tra cam bien", "", "BACK Ve  OK Home");
    return;
  }

  if (!maxFingerPresent) {
    drawInfoScreen("1.2 CHI SO SPO2", "Status : No Finger", "Dat tay len cam bien", "", "BACK Ve  OK Home");
    return;
  }

  if (validSPO2) snprintf(line1, sizeof(line1), "SpO2   : %ld %%", spo2);
  else snprintf(line1, sizeof(line1), "SpO2   : ERR");

  drawInfoScreen("1.2 CHI SO SPO2", line1, "Finger : OK", "", "BACK Ve  OK Home");
}

void drawTempScreen() {
  char line1[32];

  if (!mlxOK) {
    drawInfoScreen("1.3 NHIET DO MLX", "MLX90614: ERR", "Kiem tra cam bien", "", "BACK Ve  OK Home");
    return;
  }

  snprintf(line1, sizeof(line1), "Body  : %.2f C", bodyTemp);
  drawInfoScreen("1.3 NHIET DO MLX", line1, "Sensor: OK", "", "BACK Ve  OK Home");
}

void drawSDLogScreen() {
  if (!sdOK) {
    drawInfoScreen("2.1 TRANG THAI GHI", "SD CARD ERROR", "Khong the ghi log", "", "BACK Ve SD Menu");
    return;
  }

  char line1[32];
  snprintf(line1, sizeof(line1), "Ghi log: %s", loggingEnabled ? "BAT" : "TAT");

  drawInfoScreen("2.1 TRANG THAI GHI", line1, "OK de Bat/Tat", "", "BACK Ve SD Menu");
}

void drawSDCheckScreen() {
  if (!sdOK) {
    drawInfoScreen("2.2 KIEM TRA THE", "SD CARD ERROR", "", "", "BACK Ve SD Menu");
    return;
  }

  char line2[32];
  snprintf(line2, sizeof(line2), "Free: %lu MB", (unsigned long)sdFreeMB);

  drawInfoScreen("2.2 KIEM TRA THE", "SD OK", line2, "", "BACK Ve SD Menu");
}

void drawWiFiScreen() {
  char line1[32];
  char line2[32];
  char line3[32];

  snprintf(line1, sizeof(line1), "SSID: %s", WIFI_SSID);
  snprintf(line2, sizeof(line2), "PASS: %s", WIFI_PASS);
  snprintf(line3, sizeof(line3), "IP  : %s", WIFI_IP);

  drawInfoScreen("3.1 MANG WIFI", line1, line2, line3, "BACK Ve Menu");
}

void drawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_HOME:
      drawHomeScreen();
      break;

    case SCREEN_MAIN_MENU:
      drawMenuPage("MENU CHINH", MAIN_MENU_ITEMS, MAIN_MENU_COUNT, selectedMainMenu);
      break;

    case SCREEN_MONITOR_MENU:
      drawMenuPage("1. CHE DO XEM", MONITOR_MENU_ITEMS, MONITOR_MENU_COUNT, selectedMonitorMenu);
      break;

    case SCREEN_MONITOR_ECG:
      drawECGScreen();
      break;

    case SCREEN_MONITOR_SPO2:
      drawSpO2Screen();
      break;

    case SCREEN_MONITOR_TEMP:
      drawTempScreen();
      break;

    case SCREEN_SD_MENU:
      drawMenuPage("2. THE NHO SD", SD_MENU_ITEMS, SD_MENU_COUNT, selectedSDMenu);
      break;

    case SCREEN_SD_LOG_STATUS:
      drawSDLogScreen();
      break;

    case SCREEN_SD_CHECK:
      drawSDCheckScreen();
      break;

    case SCREEN_WIFI_IP:
      drawWiFiScreen();
      break;
  }
}

// =====================================================
// BUTTON FUNCTIONS
// =====================================================
void initButtons() {
  for (int i = 0; i < BTN_COUNT; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].stableState = digitalRead(buttons[i].pin);
    buttons[i].lastReading = buttons[i].stableState;
    buttons[i].pressedEvent = false;
    buttons[i].lastDebounceTime = 0;
  }
}

void updateButton(ButtonState &btn) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastReading) {
    btn.lastDebounceTime = millis();
    btn.lastReading = reading;
  }

  if ((millis() - btn.lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
    if (reading != btn.stableState) {
      btn.stableState = reading;

      if (btn.stableState == LOW) {
        btn.pressedEvent = true;
      }
    }
  }
}

void pollButtons() {
  for (int i = 0; i < BTN_COUNT; i++) {
    updateButton(buttons[i]);
  }
}

bool pressed(ButtonID id) {
  if (buttons[id].pressedEvent) {
    buttons[id].pressedEvent = false;
    return true;
  }

  return false;
}

// =====================================================
// MENU INPUT HELPERS
// =====================================================
void moveSelection(int &selected, int itemCount) {
  if (pressed(BTN_UP)) {
    selected--;
    if (selected < 0) selected = itemCount - 1;
  }

  if (pressed(BTN_DOWN)) {
    selected++;
    if (selected >= itemCount) selected = 0;
  }
}

// =====================================================
// MENU INPUT
// =====================================================
void handleHomeInput() {
  if (pressed(BTN_OK)) {
    currentScreen = SCREEN_MAIN_MENU;
  }
}

void handleMainMenuInput() {
  moveSelection(selectedMainMenu, MAIN_MENU_COUNT);

  if (pressed(BTN_BACK)) {
    currentScreen = SCREEN_HOME;
  }

  if (pressed(BTN_OK)) {
    switch (selectedMainMenu) {
      case 0:
        currentScreen = SCREEN_MONITOR_MENU;
        break;

      case 1:
        currentScreen = SCREEN_SD_MENU;
        break;

      case 2:
        currentScreen = SCREEN_WIFI_IP;
        break;
    }
  }
}

void handleMonitorMenuInput() {
  moveSelection(selectedMonitorMenu, MONITOR_MENU_COUNT);

  if (pressed(BTN_BACK)) {
    currentScreen = SCREEN_MAIN_MENU;
  }

  if (pressed(BTN_OK)) {
    switch (selectedMonitorMenu) {
      case 0:
        currentScreen = SCREEN_MONITOR_ECG;
        break;

      case 1:
        currentScreen = SCREEN_MONITOR_SPO2;
        break;

      case 2:
        currentScreen = SCREEN_MONITOR_TEMP;
        break;
    }
  }
}

void handleMonitorViewInput() {
  if (pressed(BTN_BACK)) {
    currentScreen = SCREEN_MONITOR_MENU;
  }

  if (pressed(BTN_OK)) {
    currentScreen = SCREEN_HOME;
  }
}

void handleSDMenuInput() {
  moveSelection(selectedSDMenu, SD_MENU_COUNT);

  if (pressed(BTN_BACK)) {
    currentScreen = SCREEN_MAIN_MENU;
  }

  if (pressed(BTN_OK)) {
    switch (selectedSDMenu) {
      case 0:
        currentScreen = SCREEN_SD_LOG_STATUS;
        break;

      case 1:
        currentScreen = SCREEN_SD_CHECK;
        break;
    }
  }
}

void handleSDDetailInput() {
  if (currentScreen == SCREEN_SD_LOG_STATUS && pressed(BTN_OK)) {
    if (sdOK) {
      loggingEnabled = !loggingEnabled;
    }
  }

  if (pressed(BTN_BACK)) {
    currentScreen = SCREEN_SD_MENU;
  }
}

void handleWiFiInput() {
  if (pressed(BTN_BACK) || pressed(BTN_OK)) {
    currentScreen = SCREEN_MAIN_MENU;
  }
}

void handleUI() {
  pollButtons();

  switch (currentScreen) {
    case SCREEN_HOME:
      handleHomeInput();
      break;

    case SCREEN_MAIN_MENU:
      handleMainMenuInput();
      break;

    case SCREEN_MONITOR_MENU:
      handleMonitorMenuInput();
      break;

    case SCREEN_MONITOR_ECG:
    case SCREEN_MONITOR_SPO2:
    case SCREEN_MONITOR_TEMP:
      handleMonitorViewInput();
      break;

    case SCREEN_SD_MENU:
      handleSDMenuInput();
      break;

    case SCREEN_SD_LOG_STATUS:
    case SCREEN_SD_CHECK:
      handleSDDetailInput();
      break;

    case SCREEN_WIFI_IP:
      handleWiFiInput();
      break;
  }

  if (millis() - lastUIRefreshMs >= UI_REFRESH_MS) {
    lastUIRefreshMs = millis();
    drawCurrentScreen();
  }
}

// =====================================================
// DEMO DATA UPDATE
// Khi tich hop that, xoa ham nay va gan bien bang sensor that
// =====================================================
void updateDemoData() {
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 500) {
    return;
  }

  lastUpdate = millis();

  ecgOK = true;
  maxOK = true;
  mlxOK = true;
  sdOK = true;

  ecgValue = 1200 + ((millis() / 20) % 300);
  ecgHeartRate = 72 + ((millis() / 1000) % 8);
  spo2 = 97 + ((millis() / 1500) % 3);
  bodyTemp = 36.4 + ((millis() / 1000) % 5) * 0.05;

  ecgHeartRateValid = true;
  validSPO2 = true;
  leadsOff = false;
  maxFingerPresent = true;

  sdFreeMB = 1024;
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  u8g2.begin();
  initButtons();

  u8g2.clearBuffer();
  drawHeader("BOOTING...");
  u8g2.drawStr(0, 26, "NHOM 10-169226");
  u8g2.drawStr(0, 38, "HEALTH MONITOR");
  u8g2.drawStr(0, 50, "MENU READY");

  u8g2.sendBuffer();

  delay(3000);
  drawCurrentScreen();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  updateDemoData();
  handleUI();
  delay(1);
}