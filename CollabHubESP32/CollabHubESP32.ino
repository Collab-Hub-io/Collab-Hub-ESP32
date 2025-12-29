#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "SioClient.h"
#include "user_script.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

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
    userScriptSetup();

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

                onConnected(username); });

    sio.on("control", onControlMessage);
    sio.on("event", onEventMessage);
    sio.on("chat", onChatMessage);

    String username = generateUsername();
    sio.begin(HUB_HOST, HUB_PORT, HUB_NAMESPACE, USE_TLS, username.c_str());
}

unsigned long lastSend = 0;

void loop()
{
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastReconnectAttempt = 0;
    static bool shouldReconnect = false;
    sio.loop();
    unsigned long now = millis();
    userScriptLoop();
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
}
