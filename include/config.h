// SPDX-FileCopyrightText: 2026 J. Feldmaier
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <Arduino.h>

// --------------------------
// Hardware pin configuration
// --------------------------
// Update these pin numbers for your exact board/wiring.

// IR LED driver control pin (via transistor/MOSFET recommended)
constexpr uint8_t PIN_IR_LED = 5;

// Set true when the IR LED is externally powered and permanently ON.
// In this mode the firmware does not toggle PIN_IR_LED and uses absolute AO values.
constexpr bool IR_LED_ALWAYS_ON = true;

// Photodiode analog output pin (use AO from module, not LM393 DO)
constexpr uint8_t PIN_SENSOR_AO = A0;

// Optional LM393 digital output pin (not used for quantitative measurement).
// Set to 0xFF to disable the pin completely. On ESP8266 do not use GPIO6-11.
constexpr uint8_t PIN_SENSOR_DO = 0xFF;

// Button pins, active LOW, use INPUT_PULLUP.
// For NodeMCU / ESP8266 these map to D5, D6, D7 and are safe to leave unconnected.
constexpr uint8_t PIN_BTN_UP = 14;
constexpr uint8_t PIN_BTN_OK = 12;
constexpr uint8_t PIN_BTN_DOWN = 13;

// SSD1306 I2C display
constexpr uint8_t OLED_ADDR = 0x3C;
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 32;

// --------------------------
// Measurement tuning
// --------------------------
constexpr uint16_t SAMPLES_PER_PHASE = 40;
constexpr uint16_t LED_SETTLE_MS = 12;
// Lower threshold so weak photodiode signals can still establish a baseline.
constexpr float MIN_VALID_SIGNAL = 2.0f;

// Typical near-infrared LED wavelength range: 850-940 nm.
constexpr uint16_t IR_WAVELENGTH_NM_MIN = 850;
constexpr uint16_t IR_WAVELENGTH_NM_MAX = 940;

// --------------------------
// WiFi configuration (embedded access point for on-device web server)
// --------------------------
constexpr const char* AP_SSID = "IR-Sunglasses-Meter";
constexpr const char* AP_PASS = "";
constexpr uint16_t WEBSERVER_PORT = 80;
