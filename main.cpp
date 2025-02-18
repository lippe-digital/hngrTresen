#include <Arduino.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>

#define NUM_LEDS 10
#define DATA_PIN 2
#define LED_TYPE SK6812
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

AsyncWebServer server(80);
WebSocketsServer webSocket(81);

bool powerOn = true;
uint8_t brightness = 127;
String currentMode = "solid";
uint32_t currentColor = 0xFF0000;
bool authenticated = false;  // Track if the UI has been unlocked
const String validPin = "1234";  // Change this to your preferred PIN

void saveSettings() {
    JsonDocument doc;
    doc["power"] = powerOn;
    doc["brightness"] = brightness;
    doc["mode"] = currentMode;
    doc["color"] = currentColor;
    File file = LittleFS.open("/config.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }
}

void loadSettings() {
    if (LittleFS.exists("/config.json")) {
        File file = LittleFS.open("/config.json", "r");
        if (!file) return;
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        if (error) return;
        powerOn = doc["power"];
        brightness = doc["brightness"];
        currentMode = doc["mode"].as<String>();
        currentColor = doc["color"];
        file.close();
    }
}

void applyLEDSettings() {
    if (!powerOn) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
    } else if (currentMode == "solid") {
        fill_solid(leds, NUM_LEDS, CRGB(currentColor));
    } else if (currentMode == "rainbow") {
        static uint8_t hue = 0;
        EVERY_N_MILLISECONDS(20) { hue++; }
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CHSV(hue + (i * 256 / NUM_LEDS), 255, 255);
        }
    }
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
    String msg = (char *)payload;

    if (msg.startsWith("pin:")) {
        String pin = msg.substring(4);
        if (pin == validPin) {
            authenticated = true;
            webSocket.sendTXT(num, "auth:ok");
        } else {
            webSocket.sendTXT(num, "auth:fail");
        }
    }

    if (authenticated) {
        if (msg == "toggle_power") powerOn = !powerOn;
        else if (msg.startsWith("brightness:")) brightness = msg.substring(11).toInt();
        else if (msg.startsWith("mode:")) currentMode = msg.substring(5);
        else if (msg.startsWith("color:")) currentColor = strtol(msg.substring(6).c_str(), NULL, 16);

        applyLEDSettings();
        saveSettings();

        JsonDocument doc;
        doc["power"] = powerOn;
        doc["brightness"] = brightness;
        doc["mode"] = currentMode;
        doc["color"] = currentColor;
        String response;
        serializeJson(doc, response);
        webSocket.sendTXT(num, response);
    }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_TEXT) {
        handleWebSocketMessage(num, payload, length);
    }
}

void setup() {
    Serial.begin(115200);

    WiFiManager wm;
    if (!wm.autoConnect("LED_Controller")) ESP.restart();

    if (!LittleFS.begin()) {
        LittleFS.format();
        LittleFS.begin();
    }
    loadSettings();

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    applyLEDSettings();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html", "<h2>Upload UI First</h2>");
        }
    });

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
    ArduinoOTA.begin();
    server.begin();
}

void loop() {
    webSocket.loop();
    ArduinoOTA.handle();
}
