# HngrTresen

Simple ESP32 c3 super mini controller for 10 sk6812rgbww leds.

It includes:
- Web UI with PIN protection
- On/Off switch, brightness control, color selection, and animation options
- OTA updates with auto-revert
- WebSocket support for real-time updates
- Persistent settings (brightness & last animation saved)
- Wifi-manager with captive portal

## Install Required Libraries

+ FastLED
+ ArduinoJson
+ ESPAsyncWebServer
+ ESPAsyncTCP
+ WebSocketsServer
+ LittleFS_esp32
+ ArduinoOTA

## Enable LittleFS Support

Install ESP32 LittleFS Data Upload Tool.\
Create a folder named data inside your project directory. 

## Uploading Files & Flashing Firmware

Upload UI Files (Use LittleFS Upload Tool)\
Flash the Firmware from Arduino IDE
