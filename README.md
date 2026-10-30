<!-- SPDX-FileCopyrightText: 2026 J. Feldmaier -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# IR Sunglasses Meter

Ein kleines Messgeraet, das die Transmission von Sonnenbrillenglaesern im nahen Infrarot (ca. 850-940 nm) bestimmt. Eine IR-LED strahlt durch das Glas auf eine Photodiode; die Firmware vergleicht das Signal mit und ohne Glas und zeigt das Ergebnis ueber ein eingebautes Web-Interface (WLAN Access Point) an.

## Hardware

- ESP8266-Board (z. B. NodeMCU Amica V2 / ESP-12F) - Standard-Target
- alternativ: Adafruit WICED Feather WiFi (Arduino-IDE-Variante)
- IR-LED (850-940 nm) mit Treiberstufe (Logic-Level-MOSFET oder NPN-Transistor)
- Photodiode bzw. IR-Sensormodul mit Analogausgang
- optional: SSD1306 OLED (128x32, I2C) und drei Taster

Verdrahtung: siehe [WIRING.md](WIRING.md).

## Firmware bauen

### PlatformIO (empfohlen)

```sh
pio run -e nodemcuv2_headless -t upload
pio device monitor
```

Pins, Messparameter und WLAN-Zugangsdaten werden in [include/config.h](include/config.h) eingestellt.

### Arduino IDE

Siehe [arduino_export/IRSunglassesMeter/README_ARDUINO.md](arduino_export/IRSunglassesMeter/README_ARDUINO.md).

## Bedienung

1. Geraet einschalten und mit dem WLAN `IR-Sunglasses-Meter` verbinden.
2. Die seriell ausgegebene IP-Adresse (Standard: `192.168.4.1`) im Browser oeffnen.
3. Baseline ohne Glas aufnehmen, dann Glas in den Strahlengang halten und messen.

Kurzanleitung als PDF: `IR_Sunglasses_Meter_Anleitung.pdf` (neu erzeugen mit `pip install -r requirements.txt` und `python make_onepager.py`).

## Entstehung

Der Code wurde mit Unterstuetzung eines KI-Coding-Assistenten erstellt. Es ist kein Code aus Drittprojekten eingebunden; verwendet werden nur die in `platformio.ini` gelisteten Bibliotheken als externe Abhaengigkeiten.

## Beitragen

Feedback, Issues und Pull Requests sind willkommen - insbesondere zu Messgenauigkeit, Kalibrierung und Hardware-Varianten.

Jede und jeder kann beitragen. Pull Requests werden oeffentlich und nach rein technischen Kriterien bewertet:

- die Firmware baut fehlerfrei (`pio run`),
- die Aenderung ist nachvollziehbar beschrieben und passt zum Projektumfang,
- neue Dateien tragen den SPDX-Header (Apache-2.0).

## Lizenz

Copyright 2026 J. Feldmaier

Lizenziert unter der Apache License 2.0 - siehe [LICENSE](LICENSE).
