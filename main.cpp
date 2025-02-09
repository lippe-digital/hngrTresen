#include <Arduino.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#define NUM_LEDS 10
#define DATA_PIN 2
#define LED_TYPE SK6812RGBW
#define COLOR_ORDER GRBW

CRGBW leds[NUM_LEDS];

AsyncWebServer server(80);
WebSocketsServer webSocket(81);

const String validPin = "1234";

bool powerOn = true;
uint8_t brightness = 127;
String currentMode = "solid";
uint32_t currentColor = 0xFF0000;

void saveSettings() {
    DynamicJsonDocument doc(512);
    doc["power"] = powerOn;
    doc["brightness"] = brightness;
    doc["mode"] = currentMode;
    doc["color"] = currentColor;

    File file = LittleFS.open("/config.json", "w");
    serializeJson(doc, file);
    file.close();
}

void loadSettings() {
    if (LittleFS.exists("/config.json")) {
        File file = LittleFS.open("/config.json", "r");
        DynamicJsonDocument doc(512);
        deserializeJson(doc, file);
        powerOn = doc["power"];
        brightness = doc["brightness"];
        currentMode = doc["mode"].as<String>();
        currentColor = doc["color"];
        file.close();
    }
}

void applyLEDSettings() {
    if (!powerOn) {
        fill_solid(leds, NUM_LEDS, CRGBW::Black);
    } else if (currentMode == "solid") {
        fill_solid(leds, NUM_LEDS, CRGBW(currentColor));
    } else if (currentMode == "rainbow") {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CHSV((i * 256 / NUM_LEDS), 255, 255);
        }
    }
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
    String msg = (char *)payload;

    if (msg == "toggle_power") {
        powerOn = !powerOn;
    } else if (msg.startsWith("brightness:")) {
        brightness = msg.substring(10).toInt();
    } else if (msg.startsWith("mode:")) {
        currentMode = msg.substring(5);
    } else if (msg.startsWith("color:")) {
        long color = strtol(msg.substring(6).c_str(), NULL, 16);
        currentColor = color;
    } else if (msg == "toggle_white") {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].white = powerOn ? 255 : 0;
        }
    }

    applyLEDSettings();
    saveSettings();
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_TEXT) handleWebSocketMessage(num, payload, length);
}

void setup() {
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness);

    if (!LittleFS.begin()) return;
    loadSettings();
    applyLEDSettings();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/validate-pin", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("pin")) {
            String pin = request->getParam("pin")->value();
            request->send(200, "text/plain", pin == validPin ? "OK" : "Denied");
        }
    });

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
    
    ArduinoOTA.onStart([]() {
        File flag = LittleFS.open("/ota_flag", "w");
        flag.print("1");
        flag.close();
    });

    ArduinoOTA.onEnd([]() {
        LittleFS.remove("/ota_flag");
    });

    ArduinoOTA.begin();
    server.begin();
}

void loop() {
    webSocket.loop();
    ArduinoOTA.handle();

    if (LittleFS.exists("/ota_flag")) {
        ESP.restart();
    }
}
