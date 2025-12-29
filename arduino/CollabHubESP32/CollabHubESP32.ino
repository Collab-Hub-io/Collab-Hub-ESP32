#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "SioClient.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
#define EVENT_PIN 0 // Change to desired GPIO pin

SioClient sio;

String generateUsername()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[20];
    snprintf(buf, sizeof(buf), "ESP-%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(buf);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("[Collab-Hub ESP32] Booting...");
    Serial.flush();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(EVENT_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("WiFi OK: ");
    Serial.println(WiFi.localIP());

    sio.onOpen([]()
               {
     String username = generateUsername();
     StaticJsonDocument<64> d1;
     d1["username"] = username;
     String s1; serializeJson(d1, s1);
     sio.emit("addUsername", s1.c_str());

     StaticJsonDocument<128> d2;
     d2["room"] = IOT_ROOM;
     String s2; serializeJson(d2, s2);
     sio.emit("joinRoom", s2.c_str());

     StaticJsonDocument<64> d3;
     d3["observe"] = true;
     String s3; serializeJson(d3, s3);
     sio.emit("observeAllControl", s3.c_str());
     sio.emit("observeAllEvents", s3.c_str());

     Serial.print("Namespace opened, joined room as username: ");
     Serial.println(username); });

    sio.on("control", [](const char *json, size_t len)
           {
     Serial.print("Control received: ");
     Serial.write(json, len);
     Serial.println();
        StaticJsonDocument<1024> doc;
     DeserializationError err = deserializeJson(doc, json, len);
     if (err) { Serial.println("control parse error"); return; }
     const char* header = doc["header"] | "";
     if (strcmp(header, "led") == 0) {
       bool on = doc["values"] | false;
       digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
       Serial.println(on ? "LED ON" : "LED OFF");
     } else {
       Serial.print("control: ");
       serializeJson(doc, Serial);
       Serial.println();
     } });

    sio.on("event", [](const char *json, size_t len)
           {
               Serial.print("Event received: ");
               Serial.write(json, len);
               Serial.println();
               // Optionally parse if needed
           });

    sio.on("chat", [](const char *json, size_t len)
           {
               Serial.print("Chat received: ");
               Serial.write(json, len);
               Serial.println();
               // Optionally parse if needed
           });

    String username = generateUsername();
    sio.begin(HUB_HOST, HUB_PORT, HUB_NAMESPACE, USE_TLS, username.c_str());
}

unsigned long lastSend = 0;

void loop()
{
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastReconnectAttempt = 0;
    static bool shouldReconnect = false;
    static bool lastEventPinState = HIGH;
    sio.loop();
    unsigned long now = millis();
    bool eventPinState = digitalRead(EVENT_PIN);
    if (lastEventPinState == HIGH && eventPinState == LOW)
    {
        // Button pressed (falling edge)
        StaticJsonDocument<128> doc;
        doc["header"] = "ESP-Event1";
        doc["target"] = "all";
        doc["mode"] = "push";
        String s;
        serializeJson(doc, s);
        Serial.print("Sending event: ");
        Serial.println(s);
        sio.emit("event", s.c_str());
    }
    lastEventPinState = eventPinState;
    // bool eventPinState = digitalRead(EVENT_PIN); // Removed duplicate declaration
    if (lastEventPinState == HIGH && eventPinState == LOW)
    {
        // Button pressed (falling edge)
        StaticJsonDocument<64> doc;
        doc["header"] = "ESP-Event1";
        String s;
        serializeJson(doc, s);
        Serial.print("Sending event: ");
        Serial.println(s);
        sio.emit("event", s.c_str());
    }
    lastEventPinState = eventPinState;
    if (!sio.connected())
    {
        if (!shouldReconnect)
        {
            Serial.println("[ESP32] Disconnected from server, will attempt reconnect...");
            shouldReconnect = true;
            lastReconnectAttempt = now;
        }
        if (shouldReconnect && (now - lastReconnectAttempt > 5000))
        {
            Serial.println("[ESP32] Attempting reconnect...");
            sio.begin(HUB_HOST, HUB_PORT, HUB_NAMESPACE, USE_TLS);
            shouldReconnect = false;
        }
        delay(100);
        return;
    }
    else
    {
        shouldReconnect = false;
    }
    if (now - lastHeartbeat > 2000)
    {
        // Serial.println("[ESP32] Loop heartbeat");
        lastHeartbeat = now;
    }
    if (now - lastSend >= CONTROL_INTERVAL_MS)
    {
        lastSend = now;
        int raw = analogRead(SENSOR_PIN);
        StaticJsonDocument<256> doc;
        doc["header"] = "Esp-Control";
        doc["values"] = 0.5;
        doc["mode"] = "push";
        doc["target"] = "all";
        String s;
        serializeJson(doc, s);
        Serial.print("Sending control JSON: ");
        Serial.println(s);
        sio.emit("control", s.c_str());
    }
}
