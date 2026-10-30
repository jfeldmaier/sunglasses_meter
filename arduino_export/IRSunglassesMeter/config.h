// SPDX-FileCopyrightText: 2026 J. Feldmaier
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <Arduino.h>

// --------------------------
// Hardware pin configuration
// --------------------------
// Update these pin numbers for your exact board/wiring.

// IR LED driver control pin (via transistor/MOSFET recommended)
const uint8_t PIN_IR_LED = 5;

// Photodiode analog output pin (use AO from module, not LM393 DO)
const uint8_t PIN_SENSOR_AO = PA0;

// Optional LM393 digital output pin (not used for quantitative measurement)
const uint8_t PIN_SENSOR_DO = 6;

// Button pins, active LOW, use INPUT_PULLUP
const uint8_t PIN_BTN_UP = 9;
const uint8_t PIN_BTN_OK = 10;
const uint8_t PIN_BTN_DOWN = 11;

// SSD1306 I2C display
const uint8_t OLED_ADDR = 0x3C;
const int OLED_WIDTH = 128;
const int OLED_HEIGHT = 32;

// --------------------------
// Measurement tuning
// --------------------------
const uint16_t SAMPLES_PER_PHASE = 40;
const uint16_t LED_SETTLE_MS = 12;
const float MIN_VALID_SIGNAL = 10.0f;

// Typical near-infrared LED wavelength range: 850-940 nm.
const uint16_t IR_WAVELENGTH_NM_MIN = 850;
const uint16_t IR_WAVELENGTH_NM_MAX = 940;

// --------------------------
// WiFi configuration (embedded access point for on-device web server)
// --------------------------
const char* AP_SSID = "IR-Sunglasses-Meter";
const char* AP_PASS = "measure123";
const uint16_t WEBSERVER_PORT = 80;
