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

const char* ssid = "YourSSID";
const char* password = "YourPASSWORD";
const String validPin = "1234";

bool powerOn = true;
uint8_t brightness = 127;
String currentMode = "solid";
uint32_t currentColor = 0xFF0000;

// 🔹 Minimal embedded upload page if UI files are missing
const char uploadPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><title>Upload UI Files</title></head>
<body>
    <h2>Upload UI Files</h2>
    <form method="POST" action="/upload" enctype="multipart/form-data">
        <input type="file" name="file">
        <input type="submit" value="Upload">
    </form>
</body>
</html>
)rawliteral";

// 🔹 Save settings to LittleFS
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

// 🔹 Load settings from LittleFS
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

// 🔹 Apply LED settings
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

// 🔹 Handle WebSocket messages
void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
    String msg = (char *)payload;
    if (msg == "toggle_power") powerOn = !powerOn;
    else if (msg.startsWith("brightness:")) brightness = msg.substring(10).toInt();
    else if (msg.startsWith("mode:")) currentMode = msg.substring(5);
    else if (msg.startsWith("color:")) currentColor = strtol(msg.substring(6).c_str(), NULL, 16);
    else if (msg == "toggle_white") for (int i = 0; i < NUM_LEDS; i++) leds[i].white = powerOn ? 255 : 0;
    applyLEDSettings();
    saveSettings();
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_TEXT) handleWebSocketMessage(num, payload, length);
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(1000); Serial.println("Connecting..."); }
    Serial.println("Connected!");

    if (!LittleFS.begin()) { Serial.println("LittleFS Mount Failed"); return; }
    loadSettings();
    applyLEDSettings();

    // 🔹 Serve UI or fallback upload page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists("/index.html")) request->send(LittleFS, "/index.html", "text/html");
        else request->send_P(200, "text/html", uploadPage);
    });

    // 🔹 File Upload Handling
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); },
        [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
            File file = LittleFS.open("/" + filename, index ? FILE_APPEND : FILE_WRITE);
            if (file) { file.write(data, len); file.close(); }
    });

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
    
    ArduinoOTA.onStart([]() { File flag = LittleFS.open("/ota_flag", "w"); flag.print("1"); flag.close(); });
    ArduinoOTA.onEnd([]() { LittleFS.remove("/ota_flag"); });
    ArduinoOTA.begin();
    
    server.begin();
}

void loop() {
    webSocket.loop();
    ArduinoOTA.handle();
    if (LittleFS.exists("/ota_flag")) ESP.restart();
}
