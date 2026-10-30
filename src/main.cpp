// SPDX-FileCopyrightText: 2026 J. Feldmaier
// SPDX-License-Identifier: Apache-2.0

#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"

#if !defined(DISABLE_OLED) && __has_include(<Adafruit_GFX.h>) && __has_include(<Adafruit_SSD1306.h>)
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define HAS_DISPLAY 1
#else
#define HAS_DISPLAY 0
#endif

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#define HAS_WIFI 1
ESP8266WebServer webserver(WEBSERVER_PORT);
#define HAS_CAL_STORE 1
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#define HAS_WIFI 1
WebServer webserver(WEBSERVER_PORT);
#define HAS_CAL_STORE 0
#elif __has_include(<WiFi101.h>)
#include <WiFi101.h>
#include <WiFiWebServer.h>
#define HAS_WIFI 1
WiFiWebServer webserver(WEBSERVER_PORT);
#define HAS_CAL_STORE 0
#else
#define HAS_WIFI 0
#define HAS_CAL_STORE 0
#endif

#if __has_include(<EEPROM.h>)
#include <EEPROM.h>
#define HAS_EEPROM 1
#else
#define HAS_EEPROM 0
#endif

#include "web_embedded.h"

namespace {

struct CalibrationData {
  uint32_t magic;
  float slope;
  float intercept;
  uint8_t points;
};

struct PersistentData {
  uint32_t magic;
  CalibrationData cal;
  float baseline;
  uint8_t baseReady;
  float knownPercent;
};

constexpr uint32_t CAL_MAGIC = 0x49524341;  // "IRCA"
constexpr uint32_t PERSIST_MAGIC = 0x49525053;  // "IRPS"
constexpr int EEPROM_ADDR = 0;
constexpr const char* DEFAULT_CAL_NAME = "autosave";
#if HAS_CAL_STORE
constexpr const char* CAL_DIR = "/cal";
constexpr const char* CAL_LAST_FILE = "/cal/last.txt";
#endif

#if HAS_DISPLAY
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
#endif

CalibrationData cal = {CAL_MAGIC, 1.0f, 0.0f, 0};

float baseSignal = 0.0f;
float lastRawTransmittance = 0.0f;
float lastCalibratedTransmittance = 0.0f;
float lastA0Value = 0.0f;
float quickReferenceSignal = 0.0f;
float lastQuickSignal = 0.0f;
float lastQuickTransmittance = 0.0f;
bool baseReady = false;
bool quickReferenceReady = false;

bool inCalibrationMode = false;
float calibrationKnownPercent = 50.0f;

constexpr uint8_t MAX_CAL_POINTS = 16;
float xRaw[MAX_CAL_POINTS] = {0};
float yKnown[MAX_CAL_POINTS] = {0};
uint8_t calPointCount = 0;

struct Button {
  uint8_t pin;
  bool stableState;
  bool prevStableState;
  uint32_t lastChangeMs;
  uint32_t pressedAtMs;
  bool longPressHandled;
};

Button btnUp{PIN_BTN_UP, true, true, 0, 0, false};
Button btnOk{PIN_BTN_OK, true, true, 0, 0, false};
Button btnDown{PIN_BTN_DOWN, true, true, 0, 0, false};

constexpr uint16_t DEBOUNCE_MS = 35;
constexpr uint16_t LONG_PRESS_MS = 900;
constexpr uint16_t STATUS_INTERVAL_MS = 800;

String serialRxLine;
uint32_t lastStatusMs = 0;
String activeCalibrationName = DEFAULT_CAL_NAME;

void processSerialCommand(String cmd);
void emitStatus();
void saveCalibration();
void loadCalibration();

void saveCalibrationToEeprom();
void loadCalibrationFromEeprom();

#if HAS_CAL_STORE
bool initCalibrationStore();
String normalizeCalibrationName(String name);
String buildCalibrationPath(const String& safeName);
bool saveCalibrationNamed(const String& name);
bool loadCalibrationNamed(const String& name);
bool loadLastSavedCalibration();
void appendCalibrationList(JsonArray arr);
bool exportCalibrationToClient(const String& name);
String readLastCalibrationName();
void writeLastCalibrationName(const String& safeName);
#endif

#if HAS_WIFI
bool wifiConnected = false;
char wifiIP[16] = "";
char wifiSsid[33] = "";

String extractCommandFromRequest() {
  if (webserver.hasArg("cmd")) return webserver.arg("cmd");
  if (!webserver.hasArg("plain")) return "";

  const String body = webserver.arg("plain");
  if (body.length() == 0) return "";

  JsonDocument doc;
  if (deserializeJson(doc, body) == DeserializationError::Ok) {
    return String(doc["cmd"] | "");
  }
  return body;
}

String extractFieldFromRequest(const char* fieldName) {
  if (webserver.hasArg(fieldName)) return webserver.arg(fieldName);
  if (!webserver.hasArg("plain")) return "";

  JsonDocument doc;
  if (deserializeJson(doc, webserver.arg("plain")) == DeserializationError::Ok) {
    return String(doc[fieldName] | "");
  }
  return "";
}

void setupWiFi() {
  wifiConnected = false;
  const bool hasPassword = AP_PASS[0] != '\0';

#if defined(ESP8266) || defined(ESP32)
  WiFi.mode(WIFI_AP);
  wifiConnected = hasPassword ? WiFi.softAP(AP_SSID, AP_PASS) : WiFi.softAP(AP_SSID);
  if (wifiConnected) {
    snprintf(wifiIP, sizeof(wifiIP), "%s", WiFi.softAPIP().toString().c_str());
  }
#else
  int status = hasPassword ? WiFi.beginAP(AP_SSID, AP_PASS) : WiFi.beginAP(AP_SSID);
  wifiConnected = (status == WL_AP_LISTENING || status == WL_CONNECTED);
  if (wifiConnected) {
    snprintf(wifiIP, sizeof(wifiIP), "%s", WiFi.localIP().toString().c_str());
  }
#endif

  if (wifiConnected) {
    snprintf(wifiSsid, sizeof(wifiSsid), "%s", AP_SSID);
#if defined(ESP8266)
    if (!MDNS.begin("sunglasses")) {
      Serial.println("mDNS init failed");
    }
#endif
    Serial.print("Access point active: ");
    Serial.println(wifiSsid);
    Serial.print("Open http://sunglasses.local");
    Serial.print(" or http://");
    Serial.print(wifiIP);
    Serial.print(":");
    Serial.println(WEBSERVER_PORT);
  } else {
    Serial.println("Access point start failed");
  }
}

void setupWebServer() {
  webserver.on("/", HTTP_GET, []() {
    webserver.send(200, "text/html; charset=utf-8", HTML_EMBEDDED);
  });

  webserver.on("/api/status", HTTP_GET, []() {
    JsonDocument doc;
    doc["raw"] = lastRawTransmittance;
    doc["cal"] = lastCalibratedTransmittance;
    doc["quickRef"] = quickReferenceSignal;
    doc["quickSig"] = lastQuickSignal;
    doc["quick"] = lastQuickTransmittance;
    doc["quickReady"] = quickReferenceReady ? 1 : 0;
    doc["baseline"] = baseSignal;
    doc["baseReady"] = baseReady ? 1 : 0;
    doc["calMode"] = inCalibrationMode ? 1 : 0;
    doc["known"] = calibrationKnownPercent;
    doc["points"] = calPointCount;
    doc["slope"] = cal.slope;
    doc["intercept"] = cal.intercept;
    doc["activeCal"] = activeCalibrationName;
    doc["wifiMode"] = "ap";
    doc["ssid"] = wifiSsid;
    doc["ip"] = wifiIP;
    String response;
    serializeJson(doc, response);
    webserver.send(200, "application/json", response);
  });

  auto handleCmd = []() {
    const String cmd = extractCommandFromRequest();
    if (cmd.length() == 0) {
      webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"missing-cmd\"}");
      return;
    }
    processSerialCommand(cmd);
    webserver.send(200, "application/json", "{\"ok\":true}");
  };

  webserver.on("/api/cmd", HTTP_POST, handleCmd);
  webserver.on("/api/cmd", HTTP_GET, handleCmd);

  auto handleCalSave = []() {
    const String name = extractFieldFromRequest("name");
    if (name.length() == 0) {
      webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"missing-name\"}");
      return;
    }
#if HAS_CAL_STORE
    const bool ok = saveCalibrationNamed(name);
#else
    activeCalibrationName = name;
    saveCalibration();
    const bool ok = true;
#endif
    if (!ok) {
      webserver.send(500, "application/json", "{\"ok\":false,\"error\":\"save-failed\"}");
      return;
    }
    emitStatus();
    JsonDocument doc;
    doc["ok"] = 1;
    doc["active"] = activeCalibrationName;
    String response;
    serializeJson(doc, response);
    webserver.send(200, "application/json", response);
  };

  auto handleCalLoad = []() {
    const String name = extractFieldFromRequest("name");
    if (name.length() == 0) {
      webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"missing-name\"}");
      return;
    }
#if HAS_CAL_STORE
    const bool ok = loadCalibrationNamed(name);
#else
    activeCalibrationName = name;
    loadCalibration();
    const bool ok = true;
#endif
    if (!ok) {
      webserver.send(404, "application/json", "{\"ok\":false,\"error\":\"not-found\"}");
      return;
    }
    emitStatus();
    JsonDocument doc;
    doc["ok"] = 1;
    doc["active"] = activeCalibrationName;
    String response;
    serializeJson(doc, response);
    webserver.send(200, "application/json", response);
  };

  webserver.on("/api/cal/save", HTTP_POST, handleCalSave);
  webserver.on("/api/cal/save", HTTP_GET, handleCalSave);
  webserver.on("/api/cal/load", HTTP_POST, handleCalLoad);
  webserver.on("/api/cal/load", HTTP_GET, handleCalLoad);

  webserver.on("/api/cal/list", HTTP_GET, []() {
    JsonDocument doc;
    doc["ok"] = 1;
    doc["active"] = activeCalibrationName;
    JsonArray items = doc["items"].to<JsonArray>();
#if HAS_CAL_STORE
    appendCalibrationList(items);
#else
    items.add(activeCalibrationName);
#endif
    String response;
    serializeJson(doc, response);
    webserver.send(200, "application/json", response);
  });

  webserver.on("/api/cal/export", HTTP_GET, []() {
    const String name = extractFieldFromRequest("name");
    if (name.length() == 0) {
      webserver.send(400, "application/json", "{\"ok\":false,\"error\":\"missing-name\"}");
      return;
    }
#if HAS_CAL_STORE
    if (!exportCalibrationToClient(name)) {
      webserver.send(404, "application/json", "{\"ok\":false,\"error\":\"not-found\"}");
    }
#else
    JsonDocument doc;
    doc["name"] = activeCalibrationName;
    doc["slope"] = cal.slope;
    doc["intercept"] = cal.intercept;
    doc["points"] = cal.points;
    doc["baseline"] = baseSignal;
    doc["baseReady"] = baseReady ? 1 : 0;
    doc["known"] = calibrationKnownPercent;
    String payload;
    serializeJsonPretty(doc, payload);
    webserver.sendHeader("Content-Disposition", "attachment; filename=\"calibration.json\"");
    webserver.send(200, "application/json", payload);
#endif
  });

  webserver.begin();
}
#endif

void saveCalibrationToEeprom() {
#if HAS_EEPROM
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(256);
#endif
  PersistentData data{};
  data.magic = PERSIST_MAGIC;
  data.cal = cal;
  data.baseline = baseSignal;
  data.baseReady = baseReady ? 1 : 0;
  data.knownPercent = calibrationKnownPercent;
  EEPROM.put(EEPROM_ADDR, data);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();
#endif
#endif
}

void loadCalibrationFromEeprom() {
#if HAS_EEPROM
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(256);
#endif
  PersistentData data{};
  EEPROM.get(EEPROM_ADDR, data);
  if (data.magic == PERSIST_MAGIC &&
      data.cal.magic == CAL_MAGIC &&
      isfinite(data.cal.slope) &&
      isfinite(data.cal.intercept) &&
      isfinite(data.baseline) &&
      isfinite(data.knownPercent)) {
    cal = data.cal;
    baseSignal = data.baseline;
    baseReady = data.baseReady != 0;
    calibrationKnownPercent = data.knownPercent;
  } else {
    // Legacy migration: older builds only stored CalibrationData.
    CalibrationData legacy{};
    EEPROM.get(EEPROM_ADDR, legacy);
    if (legacy.magic == CAL_MAGIC && isfinite(legacy.slope) && isfinite(legacy.intercept)) {
      cal = legacy;
      baseSignal = 0.0f;
      baseReady = false;
      calibrationKnownPercent = 50.0f;
      saveCalibration();
      return;
    }

    cal = {CAL_MAGIC, 1.0f, 0.0f, 0};
    baseSignal = 0.0f;
    baseReady = false;
    calibrationKnownPercent = 50.0f;
    saveCalibration();
  }
#else
  cal = {CAL_MAGIC, 1.0f, 0.0f, 0};
  baseSignal = 0.0f;
  baseReady = false;
  calibrationKnownPercent = 50.0f;
#endif
}

#if HAS_CAL_STORE
bool initCalibrationStore() {
  if (!LittleFS.begin()) return false;
  if (!LittleFS.exists(CAL_DIR)) {
    LittleFS.mkdir(CAL_DIR);
  }
  return true;
}

String normalizeCalibrationName(String name) {
  name.trim();
  if (name.length() == 0) return DEFAULT_CAL_NAME;
  String out = "";
  for (size_t i = 0; i < name.length() && out.length() < 24; ++i) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '-' || c == '_';
    out += ok ? c : '_';
  }
  if (out.length() == 0) return DEFAULT_CAL_NAME;
  return out;
}

String buildCalibrationPath(const String& safeName) {
  return String(CAL_DIR) + "/" + safeName + ".json";
}

void writeLastCalibrationName(const String& safeName) {
  File f = LittleFS.open(CAL_LAST_FILE, "w");
  if (!f) return;
  f.print(safeName);
  f.close();
}

String readLastCalibrationName() {
  if (!LittleFS.exists(CAL_LAST_FILE)) return "";
  File f = LittleFS.open(CAL_LAST_FILE, "r");
  if (!f) return "";
  String name = f.readString();
  f.close();
  name.trim();
  return normalizeCalibrationName(name);
}

bool saveCalibrationNamed(const String& name) {
  const String safeName = normalizeCalibrationName(name);
  const String path = buildCalibrationPath(safeName);

  JsonDocument doc;
  doc["name"] = safeName;
  doc["slope"] = cal.slope;
  doc["intercept"] = cal.intercept;
  doc["points"] = cal.points;
  doc["baseline"] = baseSignal;
  doc["baseReady"] = baseReady ? 1 : 0;
  doc["known"] = calibrationKnownPercent;

  File f = LittleFS.open(path, "w");
  if (!f) return false;
  if (serializeJsonPretty(doc, f) == 0) {
    f.close();
    return false;
  }
  f.close();

  activeCalibrationName = safeName;
  writeLastCalibrationName(safeName);
  saveCalibrationToEeprom();
  return true;
}

bool loadCalibrationNamed(const String& name) {
  const String safeName = normalizeCalibrationName(name);
  const String path = buildCalibrationPath(safeName);
  File f = LittleFS.open(path, "r");
  if (!f) return false;

  JsonDocument doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) {
    f.close();
    return false;
  }
  f.close();

  const float slope = doc["slope"] | NAN;
  const float intercept = doc["intercept"] | NAN;
  const float baseline = doc["baseline"] | NAN;
  const float known = doc["known"] | NAN;
  const int points = doc["points"] | 0;
  const int bReady = doc["baseReady"] | 0;

  if (!isfinite(slope) || !isfinite(intercept) || !isfinite(baseline) || !isfinite(known)) {
    return false;
  }

  cal.magic = CAL_MAGIC;
  cal.slope = slope;
  cal.intercept = intercept;
  cal.points = static_cast<uint8_t>(points < 0 ? 0 : (points > 255 ? 255 : points));
  baseSignal = baseline;
  baseReady = bReady != 0;
  calibrationKnownPercent = known;

  activeCalibrationName = safeName;
  writeLastCalibrationName(safeName);
  saveCalibrationToEeprom();
  return true;
}

bool loadLastSavedCalibration() {
  String last = readLastCalibrationName();
  if (last.length() == 0) {
    last = DEFAULT_CAL_NAME;
  }

  if (loadCalibrationNamed(last)) {
    return true;
  }

  if (last != DEFAULT_CAL_NAME && loadCalibrationNamed(DEFAULT_CAL_NAME)) {
    return true;
  }

  return false;
}

void appendCalibrationList(JsonArray arr) {
  Dir dir = LittleFS.openDir(CAL_DIR);
  while (dir.next()) {
    const String fileName = dir.fileName();
    if (!fileName.endsWith(".json")) continue;

    int slash = fileName.lastIndexOf('/');
    String base = (slash >= 0) ? fileName.substring(slash + 1) : fileName;
    base.remove(base.length() - 5);  // strip .json
    arr.add(base);
  }
}

bool exportCalibrationToClient(const String& name) {
  const String safeName = normalizeCalibrationName(name);
  const String path = buildCalibrationPath(safeName);
  File f = LittleFS.open(path, "r");
  if (!f) return false;

  webserver.sendHeader("Content-Disposition", "attachment; filename=\"cal-" + safeName + ".json\"");
  webserver.streamFile(f, "application/json");
  f.close();
  return true;
}
#endif

void saveCalibration() {
  saveCalibrationToEeprom();
#if HAS_CAL_STORE
  saveCalibrationNamed(activeCalibrationName);
#endif
}

void loadCalibration() {
#if HAS_CAL_STORE
  if (loadLastSavedCalibration()) {
    return;
  }
#endif
  loadCalibrationFromEeprom();
#if HAS_CAL_STORE
  saveCalibrationNamed(DEFAULT_CAL_NAME);
#endif
}

void resetCalibrationToDefault() {
  cal = {CAL_MAGIC, 1.0f, 0.0f, 0};
  calPointCount = 0;
  baseSignal = 0.0f;
  baseReady = false;
  calibrationKnownPercent = 50.0f;
  saveCalibration();
}

void setupPins() {
  if (IR_LED_ALWAYS_ON) {
    pinMode(PIN_IR_LED, INPUT);
  } else {
    pinMode(PIN_IR_LED, OUTPUT);
    digitalWrite(PIN_IR_LED, LOW);
  }

  pinMode(PIN_SENSOR_AO, INPUT);
  if (PIN_SENSOR_DO != 0xFF) {
    pinMode(PIN_SENSOR_DO, INPUT);
  }

  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
}

void updateButton(Button &b) {
  bool reading = digitalRead(b.pin);
  uint32_t now = millis();

  if (reading != b.stableState && (now - b.lastChangeMs) > DEBOUNCE_MS) {
    b.prevStableState = b.stableState;
    b.stableState = reading;
    b.lastChangeMs = now;

    if (!b.stableState) {
      b.pressedAtMs = now;
      b.longPressHandled = false;
    }
  }
}

bool buttonPressed(const Button &b) {
  return (b.prevStableState == true && b.stableState == false);
}

bool buttonReleased(const Button &b) {
  return (b.prevStableState == false && b.stableState == true);
}

bool buttonLongPress(Button &b) {
  if (!b.stableState && !b.longPressHandled && (millis() - b.pressedAtMs) > LONG_PRESS_MS) {
    b.longPressHandled = true;
    return true;
  }
  return false;
}

float readAveragedADC(uint8_t pin, uint16_t n) {
  uint32_t acc = 0;
  for (uint16_t i = 0; i < n; ++i) {
    acc += analogRead(pin);
    delayMicroseconds(300);
  }
  return static_cast<float>(acc) / static_cast<float>(n);
}

float captureSignal() {
  if (IR_LED_ALWAYS_ON) {
    // Always-on LED setup: use absolute sensor level and normalize via baseline.
    const float level = readAveragedADC(PIN_SENSOR_AO, SAMPLES_PER_PHASE);
    return level > 0.0f ? level : 0.0f;
  }

  digitalWrite(PIN_IR_LED, LOW);
  delay(LED_SETTLE_MS);
  const float dark = readAveragedADC(PIN_SENSOR_AO, SAMPLES_PER_PHASE);

  digitalWrite(PIN_IR_LED, HIGH);
  delay(LED_SETTLE_MS);
  const float lit = readAveragedADC(PIN_SENSOR_AO, SAMPLES_PER_PHASE);

  digitalWrite(PIN_IR_LED, LOW);

  const float signal = lit - dark;
  return signal > 0.0f ? signal : 0.0f;
}

void fitCalibration() {
  if (calPointCount < 2) {
    cal.slope = 1.0f;
    cal.intercept = 0.0f;
    cal.points = calPointCount;
    saveCalibration();
    return;
  }

  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (uint8_t i = 0; i < calPointCount; ++i) {
    sx += xRaw[i];
    sy += yKnown[i];
    sxx += xRaw[i] * xRaw[i];
    sxy += xRaw[i] * yKnown[i];
  }

  const float n = static_cast<float>(calPointCount);
  const float denom = (n * sxx) - (sx * sx);

  if (fabs(denom) < 1e-6f) {
    cal.slope = 1.0f;
    cal.intercept = 0.0f;
  } else {
    cal.slope = ((n * sxy) - (sx * sy)) / denom;
    cal.intercept = (sy - (cal.slope * sx)) / n;
  }

  cal.points = calPointCount;
  saveCalibration();
}

float applyCalibration(float rawPercent) {
  float t = cal.slope * rawPercent + cal.intercept;
  if (t < 0.0f) t = 0.0f;
  if (t > 100.0f) t = 100.0f;
  return t;
}

void drawMainScreen() {
#if HAS_DISPLAY
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IR trans");

#if HAS_WIFI
  if (wifiConnected) {
    display.setCursor(94, 0);
    display.print("AP");
  }
#endif

  display.setTextSize(1);
  display.setCursor(0, 10);
  if (baseReady) {
    display.print("CAL ");
    display.print(lastCalibratedTransmittance, 1);
    display.print("%");
  } else {
    display.print("NO REF");
  }

  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("RAW ");
  display.print(lastRawTransmittance, 1);
  display.print("%");

  display.setCursor(0, 28);
  display.print("OK=Meas Hold=Ref");

  display.display();
#endif
}

void drawCalibrationScreen() {
#if HAS_DISPLAY
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Cal mode");

  display.setCursor(0, 10);
  display.print("K ");
  display.print(calibrationKnownPercent, 1);
  display.print("% ");
  display.print("P ");
  display.print(calPointCount);

  display.setCursor(0, 20);
  display.print("m ");
  display.print(cal.slope, 3);
  display.print(" b ");
  display.print(cal.intercept, 2);

  display.setCursor(0, 28);
  display.print("UP/DN K OK+ hold exit");

  display.display();
#endif
}

void doMeasurement() {
  const float signal = captureSignal();

  if (!baseReady || baseSignal < MIN_VALID_SIGNAL) {
    lastRawTransmittance = 0.0f;
    lastCalibratedTransmittance = 0.0f;
    return;
  }

  float raw = 100.0f * signal / baseSignal;
  if (raw < 0.0f) raw = 0.0f;
  if (raw > 150.0f) raw = 150.0f;

  lastRawTransmittance = raw;
  lastCalibratedTransmittance = applyCalibration(raw);
}

void setQuickReferenceFromCurrent() {
  const float signal = captureSignal();
  if (signal >= MIN_VALID_SIGNAL) {
    quickReferenceSignal = signal;
    quickReferenceReady = true;
  }
}

void doQuickMeasurement() {
  lastQuickSignal = captureSignal();

  if (!quickReferenceReady || quickReferenceSignal < MIN_VALID_SIGNAL) {
    lastQuickTransmittance = 0.0f;
    return;
  }

  float t = 100.0f * lastQuickSignal / quickReferenceSignal;
  if (t < 0.0f) t = 0.0f;
  if (t > 150.0f) t = 150.0f;
  lastQuickTransmittance = t;
}

void resetQuickReference() {
  quickReferenceSignal = 0.0f;
  lastQuickSignal = 0.0f;
  lastQuickTransmittance = 0.0f;
  quickReferenceReady = false;
}

void setBaselineFromCurrent() {
  const float signal = captureSignal();
  if (signal >= MIN_VALID_SIGNAL) {
    baseSignal = signal;
    baseReady = true;
  }
}

void addCalibrationPoint() {
  if (!baseReady) return;

  doMeasurement();

  if (calPointCount < MAX_CAL_POINTS) {
    xRaw[calPointCount] = lastRawTransmittance;
    yKnown[calPointCount] = calibrationKnownPercent;
    ++calPointCount;
    fitCalibration();
  }
}

void emitStatus() {
  lastA0Value = readAveragedADC(PIN_SENSOR_AO, SAMPLES_PER_PHASE);

  Serial.print("{\"raw\":");
  Serial.print(lastRawTransmittance, 2);
  Serial.print(",\"cal\":");
  Serial.print(lastCalibratedTransmittance, 2);
  Serial.print(",\"a0\":");
  Serial.print(lastA0Value, 2);
  Serial.print(",\"quickRef\":");
  Serial.print(quickReferenceSignal, 2);
  Serial.print(",\"quickSig\":");
  Serial.print(lastQuickSignal, 2);
  Serial.print(",\"quick\":");
  Serial.print(lastQuickTransmittance, 2);
  Serial.print(",\"quickReady\":");
  Serial.print(quickReferenceReady ? 1 : 0);
  Serial.print(",\"baseline\":");
  Serial.print(baseSignal, 2);
  Serial.print(",\"baseReady\":");
  Serial.print(baseReady ? 1 : 0);
  Serial.print(",\"calMode\":");
  Serial.print(inCalibrationMode ? 1 : 0);
  Serial.print(",\"known\":");
  Serial.print(calibrationKnownPercent, 2);
  Serial.print(",\"points\":");
  Serial.print(calPointCount);
  Serial.println("}");
}

void printSerialHelp() {
  Serial.println("IR Sunglasses Meter");
  Serial.println("UI: OK short=measure, OK long=baseline, UP+DOWN short=cal mode");
  Serial.println("Serial commands:");
  Serial.println("  MEASURE");
  Serial.println("  BASELINE");
  Serial.println("  QUICKREF   (reference without glass)");
  Serial.println("  QUICKMEASURE (measure with glass)");
  Serial.println("  QUICKRESET");
  Serial.println("  MODE NORMAL|CAL");
  Serial.println("  SETKNOWN <0..100>");
  Serial.println("  CALADD");
  Serial.println("  CALCLEAR");
  Serial.println("  CALSAVE");
  Serial.println("  CALLOAD");
  Serial.println("  CALDEFAULT");
  Serial.println("  STATUS");
}

void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  cmd.toUpperCase();

  if (cmd == "MEASURE") {
    doMeasurement();
    emitStatus();
    return;
  }

  if (cmd == "BASELINE") {
    setBaselineFromCurrent();
    emitStatus();
    return;
  }

  if (cmd == "QUICKREF") {
    setQuickReferenceFromCurrent();
    emitStatus();
    return;
  }

  if (cmd == "QUICKMEASURE") {
    doQuickMeasurement();
    emitStatus();
    return;
  }

  if (cmd == "QUICKRESET") {
    resetQuickReference();
    emitStatus();
    return;
  }

  if (cmd == "CALADD") {
    addCalibrationPoint();
    emitStatus();
    return;
  }

  if (cmd == "CALCLEAR") {
    calPointCount = 0;
    fitCalibration();
    emitStatus();
    return;
  }

  if (cmd.startsWith("CALSAVE ")) {
    const String value = cmd.substring(8);
#if HAS_CAL_STORE
    saveCalibrationNamed(value);
#else
    activeCalibrationName = value;
    saveCalibration();
#endif
    emitStatus();
    return;
  }

  if (cmd == "CALSAVE") {
    saveCalibration();
    emitStatus();
    return;
  }

  if (cmd.startsWith("CALLOAD ")) {
    const String value = cmd.substring(8);
#if HAS_CAL_STORE
    loadCalibrationNamed(value);
#else
    activeCalibrationName = value;
    loadCalibration();
#endif
    emitStatus();
    return;
  }

  if (cmd == "CALLOAD") {
    loadCalibration();
    emitStatus();
    return;
  }

  if (cmd == "CALDEFAULT") {
    resetCalibrationToDefault();
    emitStatus();
    return;
  }

  if (cmd == "STATUS") {
    emitStatus();
    return;
  }

  if (cmd.startsWith("MODE ")) {
    if (cmd.endsWith("CAL")) {
      inCalibrationMode = true;
      emitStatus();
      return;
    }
    if (cmd.endsWith("NORMAL")) {
      inCalibrationMode = false;
      emitStatus();
      return;
    }
  }

  if (cmd.startsWith("SETKNOWN ")) {
    const String value = cmd.substring(9);
    float k = value.toFloat();
    if (k < 0.0f) k = 0.0f;
    if (k > 100.0f) k = 100.0f;
    calibrationKnownPercent = k;
    emitStatus();
    return;
  }

  Serial.print("{\"error\":\"unknown-command\",\"cmd\":\"");
  Serial.print(cmd);
  Serial.println("\"}");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;

    if (c == '\n') {
      processSerialCommand(serialRxLine);
      serialRxLine = "";
      continue;
    }

    if (serialRxLine.length() < 120) {
      serialRxLine += c;
    }
  }

  const uint32_t now = millis();
  if ((now - lastStatusMs) >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    emitStatus();
  }
}

void handleUi() {
  updateButton(btnUp);
  updateButton(btnOk);
  updateButton(btnDown);

  const bool upPress = buttonPressed(btnUp);
  const bool okPress = buttonPressed(btnOk);
  const bool downPress = buttonPressed(btnDown);

  const bool okLong = buttonLongPress(btnOk);
  const bool upLong = buttonLongPress(btnUp);
  const bool downLong = buttonLongPress(btnDown);

  if (okLong) {
    if (inCalibrationMode) {
      inCalibrationMode = false;
    } else {
      setBaselineFromCurrent();
    }
  }

  if (upLong && downLong) {
    calPointCount = 0;
    fitCalibration();
  }

  if (inCalibrationMode) {
    if (upPress) {
      calibrationKnownPercent += 0.5f;
      if (calibrationKnownPercent > 100.0f) calibrationKnownPercent = 100.0f;
    }
    if (downPress) {
      calibrationKnownPercent -= 0.5f;
      if (calibrationKnownPercent < 0.0f) calibrationKnownPercent = 0.0f;
    }
    if (okPress) {
      addCalibrationPoint();
    }
  } else {
    if (okPress) {
      doMeasurement();
    }
    if (upPress && downPress) {
      inCalibrationMode = true;
    }
  }

  btnUp.prevStableState = btnUp.stableState;
  btnOk.prevStableState = btnOk.stableState;
  btnDown.prevStableState = btnDown.stableState;
}

}  // namespace

void setup() {
  Serial.begin(SERIAL_BAUD);
  setupPins();
#if HAS_CAL_STORE
  initCalibrationStore();
#endif
  loadCalibration();

#if HAS_DISPLAY
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed, running headless");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("IR meter init...");
    display.display();
  }
#else
  Serial.println("Display disabled, running headless");
#endif

  printSerialHelp();

#if HAS_WIFI
  setupWiFi();
  setupWebServer();
#endif
}

void loop() {
  handleUi();
  handleSerialCommands();

#if HAS_WIFI
  if (wifiConnected) {
    webserver.handleClient();
#if defined(ESP8266)
    MDNS.update();
#endif
  }
#endif

  if (inCalibrationMode) {
    drawCalibrationScreen();
  } else {
    drawMainScreen();
  }

  delay(20);
}
