<!-- SPDX-FileCopyrightText: 2026 J. Feldmaier -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Wiring Guide for NodeMCU Amica V2 ESP-12F

This project is currently set up for a NodeMCU Lua Amica Modul V2 / ESP8266 ESP-12F board in headless mode with a web interface. In PlatformIO this matches the `nodemcuv2` board target. The measurement path uses an IR LED, a light sensor analog input, and the internal web UI for calibration.

## What you need

- NodeMCU 1.0 / ESP8266 board
- IR LED
- IR LED driver stage, preferably a small NPN transistor or logic-level MOSFET
- Photodiode or analog IR sensor module with analog output
- Optional 3.3 V supply if your sensor module does not run from the NodeMCU pin directly
- Jumper wires and a breadboard

## Pin mapping

The current firmware uses these pins:

- IR LED control: GPIO5, which is D1 on the NodeMCU
- Sensor analog output: A0
- Sensor digital output: not used
- Buttons: not required for web operation on the NodeMCU build

For this board, the current firmware environment is the one named `nodemcuv2_headless`.

## Important voltage limits

The ESP8266 analog input A0 is the critical part here:

- On NodeMCU Amica V2 boards, A0 is typically exposed through the board's onboard divider and is usually safe for 0 to 3.3 V at the board pin.
- The bare ESP8266 chip itself only accepts about 0 to 1.0 V at the raw ADC pin.
- If your board revision differs, verify it before connecting the sensor.

Do not connect a sensor output that can rise above the allowed ADC voltage without checking the board variant first.

## Recommended wiring

### 1. NodeMCU power

- NodeMCU 3V3 -> sensor VCC if the sensor supports 3.3 V
- NodeMCU GND -> sensor GND
- NodeMCU GND -> IR LED driver GND

### 2. IR LED drive circuit

Do not connect the IR LED directly to GPIO5.

Use this arrangement instead:

- NodeMCU D1 / GPIO5 -> gate resistor -> MOSFET gate
- MOSFET source -> GND
- MOSFET drain -> IR LED cathode
- IR LED anode -> current-limiting resistor -> 3.3 V or external LED supply
- NodeMCU GND -> MOSFET source GND and sensor GND, all tied together

A simple low-side N-channel MOSFET switch is the safest approach.

Recommended MOSFET type:

- Logic-level N-channel MOSFET that turns on well at 3.3 V gate drive
- Examples: AO3400, IRLML6344, 2N7002 for very small currents, or a similar logic-level device
- Avoid non-logic-level MOSFETs that need 5 V gate drive

Recommended passive parts:

- Gate resistor: 100 Ohm to 220 Ohm
- Gate pulldown resistor: 47 kOhm to 100 kOhm from gate to GND
- LED series resistor: sized for the IR LED current you want

Why the pulldown resistor matters:

- It keeps the MOSFET off while the NodeMCU boots
- It prevents a floating gate if the GPIO is not yet configured

Text schematic:

```
3.3 V or LED supply
	 |
	 Rled
	 |
 IR LED anode
 IR LED cathode
	 |
 MOSFET drain
 MOSFET source -------------------- GND
							 |
NodeMCU D1/GPIO5 -- Rgate -- MOSFET gate
												|
										 Rpull
												|
											 GND
```

Connection order during assembly:

1. Tie all grounds together first.
2. Connect MOSFET source to GND.
3. Connect IR LED cathode to MOSFET drain.
4. Connect IR LED anode through the series resistor to the supply.
5. Connect NodeMCU D1/GPIO5 to the MOSFET gate through the gate resistor.
6. Add the gate pulldown resistor from gate to GND.

Current tuning guidance:

- Start with a conservative LED resistor value so the LED is not overdriven
- If the signal is too weak, lower the LED resistor step by step
- Keep the LED current modest during initial tests to avoid heating and to make troubleshooting easier

Suggested starting values:

- Gate resistor for MOSFET: about 100 Ohm to 220 Ohm
- Gate pulldown resistor: about 47 kOhm to 100 kOhm
- LED series resistor: choose it for the LED current you want, typically in the range of 100 Ohm to 330 Ohm for small IR LEDs at 3.3 V, depending on LED forward voltage and desired current

If you are unsure, start with a higher resistor value and lower current.

### 3. Sensor input

- Sensor AO -> NodeMCU A0
- Sensor GND -> NodeMCU GND
- Sensor VCC -> NodeMCU 3V3 if supported

The firmware measures the difference between LED-off and LED-on readings, so the analog output is the important signal.

### 4. Optional sensor DO pin

- Leave it unconnected unless you want a simple threshold signal for debugging
- The firmware does not use it for the transmission measurement

## Optional buttons

The current NodeMCU headless build does not require buttons. For a first working setup, you can leave them disconnected and use the web interface only.

If you later want local hardware controls, the button pins in the code should be remapped to real ESP8266 GPIOs that are safe at boot. Good candidate pins on this board are D5, D6, and D7. Avoid boot-sensitive pins such as D3, D4, and D8 for first experiments.

## Practical build order

1. Wire GND and 3V3 first.
2. Wire the sensor AO to A0.
3. Wire the IR LED through the transistor or MOSFET stage.
4. Power the board by USB and confirm that the web interface starts.
5. Open the serial monitor or the AP web page and verify that the baseline and measurement values change when the IR LED is switched.

## Notes for calibration

- First set a baseline without the sunglasses in place.
- Then place the sunglasses in the measurement path and add calibration points in the web interface.
- Save the calibration so the values survive reboot.

## Troubleshooting

- If the board boots but values stay at zero, check the IR LED driver and sensor polarity.
- If the board resets when reading A0, the analog input is probably overvoltage.
- If Wi-Fi AP starts but the page is empty, make sure the firmware was built from the NodeMCU headless environment.
- If the IR LED gets hot, increase the series resistor.

## Current firmware mapping summary

- D1 / GPIO5 -> IR LED control
- A0 -> sensor analog output
- GND -> common ground for everything
- 3V3 -> sensor supply if supported

## Board-specific note

Your board, NodeMCU Lua Amica Modul V2 ESP8266 ESP-12F, is the right match for the current `nodemcuv2_headless` firmware environment. No board switch is needed unless you want to move to a different ESP8266 variant.

