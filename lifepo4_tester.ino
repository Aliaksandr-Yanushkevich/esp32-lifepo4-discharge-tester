
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// =========================
// USER SETTINGS
// =========================
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";

const char* BOT_TOKEN = "BOT_TOKEN";
const char* CHAT_ID   = "CHAT_ID";

// =====================================================
// ESP32-C3 MINI PINS
// =====================================================
static const int PIN_ADC_VBAT = 0;   // GPIO0 -> ADC input
static const int PIN_MOSFET   = 5;   // GPIO5 -> MOSFET gate

// =====================================================
// VOLTAGE DIVIDER
// 100k / 100k => factor 2.0
// =====================================================
static const float R_TOP    = 100000.0f;
static const float R_BOTTOM = 100000.0f;
static const float DIVIDER_FACTOR = (R_TOP + R_BOTTOM) / R_BOTTOM;

// =====================================================
// ADC SETTINGS
// =====================================================
static const int   ADC_SAMPLES = 64;
static const int   ADC_MAX     = 4095;
static const float ADC_REF_EST = 3.3f;

// =====================================================
// DISCHARGE SETTINGS
// =====================================================
float cutoffVoltage = 3.00f;
float hysteresis    = 0.05f;

// Calibration:
// V_real = calibA * V_measured + calibB
float calibA = 0.8938f;
float calibB = 0.0000f;

// =====================================================
// TIMERS
// =====================================================
unsigned long lastTelegramPollMs = 0;
unsigned long lastDiagMs         = 0;
unsigned long lastControlMs      = 0;

static const unsigned long TELEGRAM_POLL_INTERVAL_MS = 1200;
static const unsigned long DIAG_INTERVAL_MS          = 5000;
static const unsigned long CONTROL_INTERVAL_MS       = 500;

// =====================================================
// STATE
// =====================================================
bool dischargeEnabled = false;
bool mosfetState      = false;
bool cutoffLatched    = false;

// =====================================================
// OBJECTS
// =====================================================
WiFiClientSecure securedClient;
UniversalTelegramBot bot(BOT_TOKEN, securedClient);
Preferences prefs;

// =====================================================
// FORWARD DECLARATIONS
// =====================================================
void connectWiFi();
void setupTelegram();

void loadPreferences();
void saveCutoff();

void setMosfet(bool on);

uint16_t readAdcAverageRaw();
float readAdcVoltage();
float readBatteryVoltageRaw();
float readBatteryVoltageCalibrated();

void controlDischarge(float vbat);
void printDiagnostic();
void sendStatusToTelegram(const String& reason);
void sendTelegramMessage(const String& text);

void handleTelegramMessages(int numNewMessages);
void processTelegramInput(const String& chat_id, String text);

void sendMainMenu(const String& chat_id, const String& title = "Main menu");
void sendCutoffMenu(const String& chat_id);
String getMainMenuKeyboard();
String getCutoffMenuKeyboard();

String stateToString();
String formatFloat(float value, int digits = 3);

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==========================================");
  Serial.println("LiFePO4 Discharge Tester - ESP32-C3 Mini");
  Serial.println("No spam edition");
  Serial.println("==========================================");

  pinMode(PIN_MOSFET, OUTPUT);
  digitalWrite(PIN_MOSFET, LOW);
  mosfetState = false;

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ADC_VBAT, ADC_11db);

  loadPreferences();
  connectWiFi();
  setupTelegram();

  printDiagnostic();

  sendTelegramMessage(
    "✅ ESP32-C3 tester started\n"
    "Voltage: " + formatFloat(readBatteryVoltageCalibrated(), 3) + " V\n"
    "Cutoff: " + formatFloat(cutoffVoltage, 3) + " V\n"
    "Hysteresis: " + formatFloat(hysteresis, 3) + " V\n"
    "Calibration A: " + formatFloat(calibA, 4) + "\n"
    "Calibration B: " + formatFloat(calibB, 4)
  );

  sendMainMenu(CHAT_ID, "Ready");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection, reconnecting...");
    connectWiFi();
  }

  if (now - lastControlMs >= CONTROL_INTERVAL_MS) {
    lastControlMs = now;
    float vbat = readBatteryVoltageCalibrated();
    controlDischarge(vbat);
  }

  if (now - lastDiagMs >= DIAG_INTERVAL_MS) {
    lastDiagMs = now;
    printDiagnostic();
  }

  if (now - lastTelegramPollMs >= TELEGRAM_POLL_INTERVAL_MS) {
    lastTelegramPollMs = now;

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleTelegramMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}

// =====================================================
// WIFI / TELEGRAM
// =====================================================
void connectWiFi() {
  Serial.printf("[WIFI] Connecting to %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempt++;

    if (attempt >= 40) {
      Serial.println("\n[WIFI] Reconnect attempt...");
      WiFi.disconnect(true, true);
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      attempt = 0;
    }
  }

  Serial.println();
  Serial.println("[WIFI] Connected");
  Serial.print("[WIFI] IP: ");
  Serial.println(WiFi.localIP());
}

void setupTelegram() {
  securedClient.setInsecure();
  Serial.println("[TG] Telegram client ready");
}

void sendTelegramMessage(const String& text) {
  bool ok = bot.sendMessage(CHAT_ID, text, "");
  Serial.printf("[TG] sendMessage: %s\n", ok ? "OK" : "FAIL");
}

String getMainMenuKeyboard() {
  return
    "["
      "[\"▶ Start\",\"⏹ Stop\"],"
      "[\"📊 Status\",\"🧪 Diag\"],"
      "[\"⚙ Cutoff\",\"❓ Help\"]"
    "]";
}

String getCutoffMenuKeyboard() {
  return
    "["
      "[\"2.80 V\",\"2.90 V\"],"
      "[\"3.00 V\",\"3.05 V\"],"
      "[\"3.10 V\",\"3.20 V\"],"
      "[\"3.30 V\",\"⬅ Back\"]"
    "]";
}

void sendMainMenu(const String& chat_id, const String& title) {
  bool ok = bot.sendMessageWithReplyKeyboard(
    chat_id,
    "🔋 " + title + "\n"
    "Voltage: " + formatFloat(readBatteryVoltageCalibrated(), 3) + " V\n"
    "Cutoff: " + formatFloat(cutoffVoltage, 3) + " V\n"
    "State: " + stateToString(),
    "",
    getMainMenuKeyboard(),
    true,
    false,
    false
  );
  Serial.printf("[TG] send main menu: %s\n", ok ? "OK" : "FAIL");
}

void sendCutoffMenu(const String& chat_id) {
  bool ok = bot.sendMessageWithReplyKeyboard(
    chat_id,
    "⚙ Select cutoff voltage\n"
    "Current: " + formatFloat(cutoffVoltage, 3) + " V",
    "",
    getCutoffMenuKeyboard(),
    true,
    false,
    false
  );
  Serial.printf("[TG] send cutoff menu: %s\n", ok ? "OK" : "FAIL");
}

void handleTelegramMessages(int numNewMessages) {
  Serial.printf("[TG] New messages: %d\n", numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text    = bot.messages[i].text;
    String from    = bot.messages[i].from_name;

    Serial.println("--------------------------------------");
    Serial.printf("[TG] From: %s\n", from.c_str());
    Serial.printf("[TG] Chat: %s\n", chat_id.c_str());
    Serial.printf("[TG] Text: %s\n", text.c_str());

    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      Serial.println("[TG] Unauthorized chat");
      continue;
    }

    processTelegramInput(chat_id, text);
  }
}

void processTelegramInput(const String& chat_id, String text) {
  text.trim();

  if (text == "/start" || text == "▶ Start") {
    dischargeEnabled = true;
    cutoffLatched = false;

    bot.sendMessage(chat_id, "▶️ Discharge enabled", "");
    Serial.println("[CMD] START");
    sendMainMenu(chat_id, "Discharge enabled");
    return;
  }

  if (text == "/stop" || text == "⏹ Stop") {
    dischargeEnabled = false;
    setMosfet(false);

    bot.sendMessage(chat_id, "⏹ Discharge stopped", "");
    Serial.println("[CMD] STOP");
    sendMainMenu(chat_id, "Discharge stopped");
    return;
  }

  if (text == "/status" || text == "📊 Status") {
    sendStatusToTelegram("Manual status");
    Serial.println("[CMD] STATUS");
    return;
  }

  if (text == "/diag" || text == "🧪 Diag") {
    uint16_t adcRaw = readAdcAverageRaw();
    float vadc      = (adcRaw / (float)ADC_MAX) * ADC_REF_EST;
    float vraw      = vadc * DIVIDER_FACTOR;
    float vcal      = calibA * vraw + calibB;

    String msg;
    msg += "🧪 Diagnostic\n";
    msg += "ADC raw: " + String(adcRaw) + "\n";
    msg += "ADC V: " + formatFloat(vadc, 4) + " V\n";
    msg += "Raw Vbat: " + formatFloat(vraw, 4) + " V\n";
    msg += "Cal Vbat: " + formatFloat(vcal, 4) + " V\n";
    msg += "Cutoff: " + formatFloat(cutoffVoltage, 3) + " V\n";
    msg += "Hyst: " + formatFloat(hysteresis, 3) + " V\n";
    msg += "Enabled: " + String(dischargeEnabled ? "YES" : "NO") + "\n";
    msg += "MOSFET: " + String(mosfetState ? "ON" : "OFF") + "\n";
    msg += "Latched: " + String(cutoffLatched ? "YES" : "NO") + "\n";
    msg += "calA: " + formatFloat(calibA, 5) + "\n";
    msg += "calB: " + formatFloat(calibB, 5);

    bot.sendMessage(chat_id, msg, "");
    Serial.println("[CMD] DIAG");
    return;
  }

  if (text == "⚙ Cutoff") {
    Serial.println("[CMD] OPEN CUTOFF MENU");
    sendCutoffMenu(chat_id);
    return;
  }

  if (text == "2.80 V" || text == "2.90 V" || text == "3.00 V" ||
      text == "3.05 V" || text == "3.10 V" || text == "3.20 V" ||
      text == "3.30 V") {

    float newCutoff = text.substring(0, 4).toFloat();
    cutoffVoltage = newCutoff;
    saveCutoff();

    bot.sendMessage(chat_id, "✅ New cutoff: " + formatFloat(cutoffVoltage, 3) + " V", "");
    Serial.printf("[CMD] NEW CUTOFF -> %.3f V\n", cutoffVoltage);
    sendMainMenu(chat_id, "Cutoff updated");
    return;
  }

  if (text == "⬅ Back") {
    Serial.println("[CMD] BACK");
    sendMainMenu(chat_id, "Main menu");
    return;
  }

  if (text == "/help" || text == "❓ Help") {
    String msg;
    msg += "Use the buttons below.\n\n";
    msg += "Buttons:\n";
    msg += "▶ Start - enable discharge\n";
    msg += "⏹ Stop - stop discharge\n";
    msg += "📊 Status - current values\n";
    msg += "🧪 Diag - detailed diagnostics\n";
    msg += "⚙ Cutoff - choose cutoff voltage\n\n";
    msg += "Calibration:\n";
    msg += "A = " + formatFloat(calibA, 4) + "\n";
    msg += "B = " + formatFloat(calibB, 4);
    bot.sendMessage(chat_id, msg, "");
    Serial.println("[CMD] HELP");
    return;
  }

  bot.sendMessage(chat_id, "Unknown input. Use the buttons below.", "");
  Serial.println("[CMD] UNKNOWN INPUT");
  sendMainMenu(chat_id, "Main menu");
}

// =====================================================
// PREFERENCES
// =====================================================
void loadPreferences() {
  prefs.begin("tester", false);
  cutoffVoltage = prefs.getFloat("cutoff", 3.00f);

  Serial.println("[NVS] Preferences loaded");
  Serial.printf("[NVS] cutoff = %.3f\n", cutoffVoltage);
  Serial.printf("[CFG] calA   = %.5f\n", calibA);
  Serial.printf("[CFG] calB   = %.5f\n", calibB);
}

void saveCutoff() {
  prefs.putFloat("cutoff", cutoffVoltage);
  Serial.println("[NVS] Cutoff saved");
}

// =====================================================
// HARDWARE CONTROL
// =====================================================
void setMosfet(bool on) {
  if (mosfetState == on) {
    return;
  }

  mosfetState = on;
  digitalWrite(PIN_MOSFET, on ? HIGH : LOW);
  Serial.printf("[HW] MOSFET -> %s\n", on ? "ON" : "OFF");
}

// =====================================================
// ADC / VOLTAGE
// =====================================================
uint16_t readAdcAverageRaw() {
  uint32_t sum = 0;

  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PIN_ADC_VBAT);
    delayMicroseconds(300);
  }

  return (uint16_t)(sum / ADC_SAMPLES);
}

float readAdcVoltage() {
  uint16_t adcRaw = readAdcAverageRaw();
  return (adcRaw / (float)ADC_MAX) * ADC_REF_EST;
}

float readBatteryVoltageRaw() {
  float vadc = readAdcVoltage();
  return vadc * DIVIDER_FACTOR;
}

float readBatteryVoltageCalibrated() {
  float vraw = readBatteryVoltageRaw();
  return calibA * vraw + calibB;
}

// =====================================================
// DISCHARGE LOGIC
// =====================================================
void controlDischarge(float vbat) {
  if (!dischargeEnabled) {
    if (mosfetState) {
      setMosfet(false);
      Serial.println("[CTRL] Discharge disabled -> MOSFET OFF");
    }
    return;
  }

  if (mosfetState && vbat <= cutoffVoltage) {
    setMosfet(false);
    cutoffLatched = true;

    Serial.printf("[CTRL] CUTOFF triggered at %.3f V\n", vbat);

    sendTelegramMessage(
      "🛑 Cutoff reached\n"
      "Voltage: " + formatFloat(vbat, 3) + " V\n"
      "MOSFET: OFF"
    );

    sendMainMenu(CHAT_ID, "Cutoff reached");
    return;
  }

  if (!mosfetState) {
    if (!cutoffLatched) {
      setMosfet(true);
      delay(50);
      Serial.printf("[CTRL] MOSFET ON, V=%.3f V\n", vbat);
      return;
    }

    if (vbat > (cutoffVoltage + hysteresis)) {
      cutoffLatched = false;
      setMosfet(true);
      delay(50);
      Serial.printf("[CTRL] MOSFET ON after recovery, V=%.3f V\n", vbat);

      sendTelegramMessage(
        "▶️ Discharge resumed\n"
        "Voltage: " + formatFloat(vbat, 3) + " V"
      );
      sendMainMenu(CHAT_ID, "Recovered above hysteresis");
      return;
    }
  }
}

// =====================================================
// DIAGNOSTICS / STATUS
// =====================================================
void printDiagnostic() {
  uint16_t adcRaw = readAdcAverageRaw();
  float vadc      = (adcRaw / (float)ADC_MAX) * ADC_REF_EST;
  float vraw      = vadc * DIVIDER_FACTOR;
  float vcal      = calibA * vraw + calibB;

  Serial.println("--------------- DIAG ----------------");
  Serial.printf("[DIAG] ADC raw    = %u\n", adcRaw);
  Serial.printf("[DIAG] ADC volts  = %.5f V\n", vadc);
  Serial.printf("[DIAG] Vbat raw   = %.5f V\n", vraw);
  Serial.printf("[DIAG] Vbat cal   = %.5f V\n", vcal);
  Serial.printf("[DIAG] Cutoff     = %.5f V\n", cutoffVoltage);
  Serial.printf("[DIAG] Hysteresis = %.5f V\n", hysteresis);
  Serial.printf("[DIAG] Enabled    = %s\n", dischargeEnabled ? "YES" : "NO");
  Serial.printf("[DIAG] MOSFET     = %s\n", mosfetState ? "ON" : "OFF");
  Serial.printf("[DIAG] Latched    = %s\n", cutoffLatched ? "YES" : "NO");
  Serial.printf("[DIAG] calA       = %.5f\n", calibA);
  Serial.printf("[DIAG] calB       = %.5f\n", calibB);
  Serial.println("-------------------------------------");
}

void sendStatusToTelegram(const String& reason) {
  float vcal = readBatteryVoltageCalibrated();
  float vraw = readBatteryVoltageRaw();

  String msg;
  msg += "📊 " + reason + "\n";
  msg += "Voltage: " + formatFloat(vcal, 3) + " V\n";
  msg += "Raw: " + formatFloat(vraw, 3) + " V\n";
  msg += "Cutoff: " + formatFloat(cutoffVoltage, 3) + " V\n";
  msg += "State: " + stateToString() + "\n";
  msg += "MOSFET: " + String(mosfetState ? "ON" : "OFF") + "\n";
  msg += "Latched: " + String(cutoffLatched ? "YES" : "NO");

  bot.sendMessage(CHAT_ID, msg, "");
  Serial.println("[TG] status sent");
}

String stateToString() {
  if (!dischargeEnabled) return "IDLE";
  if (cutoffLatched && !mosfetState) return "CUTOFF_LATCHED";
  if (mosfetState) return "DISCHARGING";
  return "WAITING";
}

String formatFloat(float value, int digits) {
  char buf[32];
  dtostrf(value, 0, digits, buf);
  return String(buf);
}
