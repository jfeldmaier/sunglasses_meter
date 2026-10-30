// SPDX-FileCopyrightText: 2026 J. Feldmaier
// SPDX-License-Identifier: Apache-2.0

#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#ifndef nullptr
#define nullptr NULL
#endif

#if defined(ARDUINO_ARCH_WICED)
typedef HardwareSPI SPIClass;
#define HAS_OLED 0
#else
#define HAS_OLED 1
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

#include <adafruit_feather.h>
#include <adafruit_featherap.h>
#include <adafruit_http_server.h>

#define HAS_WIFI 1
#define HAS_EEPROM 0

#include "config.h"
#include "web_embedded.h"

namespace {

struct CalibrationData {
  uint32_t magic;
  float slope;
  float intercept;
  uint8_t points;
};

const uint32_t CAL_MAGIC = 0x49524341;  // "IRCA"
const int EEPROM_ADDR = 0;

#if HAS_OLED
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
#endif

CalibrationData cal = {CAL_MAGIC, 1.0f, 0.0f, 0};

float baseSignal = 0.0f;
float lastRawTransmittance = 0.0f;
float lastCalibratedTransmittance = 0.0f;
bool baseReady = false;

bool inCalibrationMode = false;
float calibrationKnownPercent = 50.0f;

const uint8_t MAX_CAL_POINTS = 16;
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

Button btnUp = {PIN_BTN_UP, true, true, 0, 0, false};
Button btnOk = {PIN_BTN_OK, true, true, 0, 0, false};
Button btnDown = {PIN_BTN_DOWN, true, true, 0, 0, false};

const uint16_t DEBOUNCE_MS = 35;
const uint16_t LONG_PRESS_MS = 900;
const uint16_t STATUS_INTERVAL_MS = 800;

String serialRxLine;
uint32_t lastStatusMs = 0;

#if HAS_WIFI
bool wifiConnected = false;
char wifiIP[16] = "";
char wifiSsid[33] = "";

IPAddress apIP(192, 168, 2, 1);
IPAddress apGateway(192, 168, 2, 1);
IPAddress apNetmask(255, 255, 255, 0);

void processSerialCommand(String cmd);

void page_root(const char* url, const char* query, httppage_request_t* http_request);
void page_status(const char* url, const char* query, httppage_request_t* http_request);
void page_cmd(const char* url, const char* query, httppage_request_t* http_request);
void page_upload(const char* url, const char* query, httppage_request_t* http_request);
void page_not_found(const char* url, const char* query, httppage_request_t* http_request);

HTTPPage pages[] = {
  HTTPPage("/", HTTP_MIME_TEXT_HTML, page_root),
  HTTPPage("/api/status", HTTP_MIME_JSON, page_status),
  HTTPPage("/api/cmd", HTTP_MIME_JSON, page_cmd),
  HTTPPage("/api/upload", HTTP_MIME_JSON, page_upload),
  HTTPPage("/404.html", HTTP_MIME_TEXT_HTML, page_not_found)
};

uint8_t pagecount = sizeof(pages) / sizeof(HTTPPage);
AdafruitHTTPServer httpserver(pagecount, WIFI_INTERFACE_SOFTAP);

void page_root(const char* url, const char* query, httppage_request_t* http_request) {
  (void) url;
  (void) query;
  (void) http_request;
  httpserver.print(HTML_EMBEDDED);
}

void page_status(const char* url, const char* query, httppage_request_t* http_request) {
  (void) url;
  (void) query;
  (void) http_request;
  httpserver.print("{");
  httpserver.print("\"raw\":"); httpserver.print(lastRawTransmittance, 2);
  httpserver.print(",\"cal\":"); httpserver.print(lastCalibratedTransmittance, 2);
  httpserver.print(",\"baseline\":"); httpserver.print(baseSignal, 2);
  httpserver.print(",\"baseReady\":"); httpserver.print(baseReady ? 1 : 0);
  httpserver.print(",\"calMode\":"); httpserver.print(inCalibrationMode ? 1 : 0);
  httpserver.print(",\"known\":"); httpserver.print(calibrationKnownPercent, 2);
  httpserver.print(",\"points\":"); httpserver.print(calPointCount);
  httpserver.print(",\"slope\":"); httpserver.print(cal.slope, 4);
  httpserver.print(",\"intercept\":"); httpserver.print(cal.intercept, 4);
  httpserver.print(",\"wifiMode\":\"ap\"");
  httpserver.print(",\"ssid\":\""); httpserver.print(wifiSsid); httpserver.print("\"");
  httpserver.print(",\"ip\":\""); httpserver.print(wifiIP); httpserver.print("\"");
  httpserver.print("}");
}

void page_cmd(const char* url, const char* query, httppage_request_t* http_request) {
  (void) url;
  (void) http_request;

  String q = query ? String(query) : String("");
  int p = q.indexOf("cmd=");
  if (p >= 0) {
    String cmd = q.substring(p + 4);
    cmd.replace("%20", " ");
    cmd.replace("+", " ");
    processSerialCommand(cmd);
    httpserver.print("{\"ok\":true}");
  } else {
    httpserver.print("{\"ok\":false,\"error\":\"missing-cmd\"}");
  }
}

void page_not_found(const char* url, const char* query, httppage_request_t* http_request) {
  (void) url;
  (void) query;
  (void) http_request;
  httpserver.print("{\"ok\":false,\"error\":\"not-found\"}");
}

uint8_t otaBuffer[256];
uint16_t otaBufIdx = 0;
uint32_t otaTotalBytes = 0;
bool otaInProgress = false;

void page_upload(const char* url, const char* query, httppage_request_t* http_request) {
  (void) url;
  (void) query;
  (void) http_request;
  
  httpserver.print("{\"ok\":true,\"msg\":\"OTA-Upload-Handler aktiv\"}");
}

void setupWiFi() {
  wifiConnected = false;

  FeatherAP.err_actions(true, false);
  FeatherAP.begin(apIP, apGateway, apNetmask, 1);

  wifiConnected = FeatherAP.start(AP_SSID, AP_PASS, ENC_TYPE_WPA2_AES);

  if (wifiConnected) {
    strncpy(wifiSsid, AP_SSID, sizeof(wifiSsid) - 1);
    wifiSsid[sizeof(wifiSsid) - 1] = 0;

    IPAddress ip = Feather.localIP();
    snprintf(wifiIP, sizeof(wifiIP), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    Serial.print("Access point active: ");
    Serial.println(wifiSsid);
    Serial.print("Open http://");
    Serial.print(wifiIP);
    Serial.print(":");
    Serial.println(WEBSERVER_PORT);
  } else {
    Serial.print("Access point start failed: ");
    Serial.println(Feather.errstr());
  }
}

void setupWebServer() {
  httpserver.err_actions(true, false);
  httpserver.addPages(pages, pagecount);
  httpserver.begin(WEBSERVER_PORT, 8);
}
#endif

void saveCalibration() {
#if HAS_EEPROM
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(64);
#endif
  EEPROM.put(EEPROM_ADDR, cal);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();
#endif
#endif
}

void loadCalibration() {
#if HAS_EEPROM
#if defined(ESP8266) || defined(ESP32)
  EEPROM.begin(64);
#endif
  CalibrationData temp;
  temp.magic = 0;
  temp.slope = 1.0f;
  temp.intercept = 0.0f;
  temp.points = 0;
  EEPROM.get(EEPROM_ADDR, temp);
  if (temp.magic == CAL_MAGIC && isfinite(temp.slope) && isfinite(temp.intercept)) {
    cal = temp;
  } else {
    cal.magic = CAL_MAGIC;
    cal.slope = 1.0f;
    cal.intercept = 0.0f;
    cal.points = 0;
    saveCalibration();
  }
#else
  cal.magic = CAL_MAGIC;
  cal.slope = 1.0f;
  cal.intercept = 0.0f;
  cal.points = 0;
#endif
}

void setupPins() {
  pinMode(PIN_IR_LED, OUTPUT);
  digitalWrite(PIN_IR_LED, LOW);

  pinMode(PIN_SENSOR_AO, INPUT);
  pinMode(PIN_SENSOR_DO, INPUT);

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
#if HAS_OLED
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
#if HAS_OLED
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
  Serial.print("{\"raw\":");
  Serial.print(lastRawTransmittance, 2);
  Serial.print(",\"cal\":");
  Serial.print(lastCalibratedTransmittance, 2);
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
  Serial.println("  MODE NORMAL|CAL");
  Serial.println("  SETKNOWN <0..100>");
  Serial.println("  CALADD");
  Serial.println("  CALCLEAR");
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
  loadCalibration();

#if HAS_OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IR meter init...");
  display.display();
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
    // WICED HTTP server runs in firmware task context.
  }
#endif

  if (inCalibrationMode) {
    drawCalibrationScreen();
  } else {
    drawMainScreen();
  }

  delay(20);
}

