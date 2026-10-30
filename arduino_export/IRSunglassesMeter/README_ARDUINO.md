<!-- SPDX-FileCopyrightText: 2026 J. Feldmaier -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Arduino Export: IRSunglassesMeter

## Inhalt

- IRSunglassesMeter.ino
- config.h
- web_embedded.h

## In Arduino IDE oeffnen

1. Arduino IDE starten.
2. Datei > Open und `IRSunglassesMeter.ino` waehlen.
3. Board auswaehlen: `Adafruit WICED Feather WiFi`.
4. Port auswaehlen (Tools > Port).
5. Upload starten.

## Benoetigte Bibliotheken

Installiere ueber Bibliotheksmanager:

- Adafruit GFX Library
- Adafruit SSD1306
- ArduinoJson

## AP-Zugangsdaten

In `config.h` setzen:

- `AP_SSID`
- `AP_PASS`

Danach ist das Webinterface unter der seriell ausgegebenen AP-IP erreichbar.
