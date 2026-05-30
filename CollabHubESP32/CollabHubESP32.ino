#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "SioClient.h"
#include "user_script.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

SioClient sio;

struct AuthState
{
    String accessToken;
    String refreshToken;
    String username;
    String mode;
};

AuthState authState;

String generateUsername()
{
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[20];
    // Keep guest username candidate server-valid (lowercase + underscore).
    snprintf(buf, sizeof(buf), "esp32_%02x%02x%02x", mac[3], mac[4], mac[5]);
    return String(buf);
}

static String normalizeGuestUsernameCandidate(const String &raw)
{
    String out = "";
    out.reserve(raw.length());

    for (size_t i = 0; i < raw.length(); ++i)
    {
        char c = raw[i];
        if (c >= 'A' && c <= 'Z')
        {
            out += (char)(c + ('a' - 'A'));
        }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
        {
            out += c;
        }
        else if (c == '-' || c == ' ')
        {
            out += '_';
        }
    }

    while (out.indexOf("__") >= 0)
    {
        out.replace("__", "_");
    }

    if (out.startsWith("_"))
    {
        out.remove(0, 1);
    }
    if (out.endsWith("_"))
    {
        out.remove(out.length() - 1);
    }

    if (out.length() < 2)
    {
        return String("esp32") + generateUsername().substring(5);
    }

    if (out.length() > 32)
    {
        out = out.substring(0, 32);
    }

    return out;
}

static bool postJson(const String &path, const String &payload, int &statusCode, String &responseBody)
{
    HTTPClient http;
    const String url = String(USE_TLS ? "https://" : "http://") + HUB_HOST + ":" + String(HUB_PORT) + path;

#if USE_TLS
    WiFiClientSecure client;
    client.setInsecure();
    if (!http.begin(client, url))
    {
        Serial.printf("[auth] http_begin_failed: %s\n", url.c_str());
        return false;
    }
#else
    WiFiClient client;
    if (!http.begin(client, url))
    {
        Serial.printf("[auth] http_begin_failed: %s\n", url.c_str());
        return false;
    }
#endif

    http.addHeader("Content-Type", "application/json");
    statusCode = http.POST(payload);
    responseBody = http.getString();
    if (statusCode <= 0)
    {
        Serial.printf("[auth] http_post_failed (%d): %s\n", statusCode, url.c_str());
    }
    http.end();
    return statusCode > 0;
}

static bool parseAuthTokens(const String &responseBody, AuthState &state)
{
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, responseBody) != DeserializationError::Ok)
    {
        return false;
    }

    const char *accessToken = doc["accessToken"];
    const char *refreshToken = doc["refreshToken"];
    if (!accessToken || !refreshToken)
    {
        return false;
    }

    state.accessToken = accessToken;
    state.refreshToken = refreshToken;

    const char *username = doc["username"];
    if (username && strlen(username) > 0)
    {
        state.username = username;
    }
    return true;
}

static bool createGuestSession(AuthState &state)
{
    String payload = "{}";
    String preferredUsername = "";
    if (strlen(HUB_AUTH_USERNAME) > 0)
    {
        preferredUsername = normalizeGuestUsernameCandidate(String(HUB_AUTH_USERNAME));
    }
    else if (strlen(HUB_DEVICE_USERNAME) > 0)
    {
        preferredUsername = normalizeGuestUsernameCandidate(String(HUB_DEVICE_USERNAME));
    }
    else
    {
        preferredUsername = generateUsername();
    }

    if (preferredUsername.length() > 0)
    {
        DynamicJsonDocument req(256);
        req["username"] = preferredUsername;
        serializeJson(req, payload);
    }

    int statusCode = 0;
    String responseBody;
    if (!postJson("/api/v1/auth/guest", payload, statusCode, responseBody))
    {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300)
    {
        if (statusCode == 404)
        {
            Serial.println("[auth] guest_unavailable: server does not support /auth/guest");
        }
        else
        {
            Serial.printf("[auth] guest_failed (%d): %s\n", statusCode, responseBody.c_str());
        }
        return false;
    }

    state.mode = "guest";
    return parseAuthTokens(responseBody, state);
}

static bool loginWithPassword(AuthState &state)
{
    if (strlen(HUB_AUTH_USERNAME) == 0 || strlen(HUB_AUTH_PASSWORD) == 0)
    {
        return false;
    }

    DynamicJsonDocument req(256);
    req["username"] = HUB_AUTH_USERNAME;
    req["password"] = HUB_AUTH_PASSWORD;

    String payload;
    serializeJson(req, payload);

    int statusCode = 0;
    String responseBody;
    if (!postJson("/api/v1/auth/login", payload, statusCode, responseBody))
    {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300)
    {
        Serial.printf("[auth] login_failed (%d): %s\n", statusCode, responseBody.c_str());
        return false;
    }

    state.mode = "password";
    state.username = HUB_AUTH_USERNAME;
    return parseAuthTokens(responseBody, state);
}

static bool refreshSession(AuthState &state)
{
    if (state.refreshToken.length() == 0)
    {
        return false;
    }

    DynamicJsonDocument req(512);
    req["refreshToken"] = state.refreshToken;

    String payload;
    serializeJson(req, payload);

    int statusCode = 0;
    String responseBody;
    if (!postJson("/api/v1/auth/refresh", payload, statusCode, responseBody))
    {
        return false;
    }

    if (statusCode < 200 || statusCode >= 300)
    {
        Serial.printf("[auth] refresh_failed (%d): %s\n", statusCode, responseBody.c_str());
        return false;
    }

    return parseAuthTokens(responseBody, state);
}

static void connectSocketWithAuth()
{
    if (authState.username.length() == 0)
    {
        if (strlen(HUB_DEVICE_USERNAME) > 0)
        {
            authState.username = HUB_DEVICE_USERNAME;
        }
        else
        {
            authState.username = generateUsername();
        }
    }

    sio.begin(
        HUB_HOST,
        HUB_PORT,
        HUB_NAMESPACE,
        USE_TLS,
        authState.username.c_str(),
        authState.accessToken.length() > 0 ? authState.accessToken.c_str() : nullptr);
}

static bool establishInitialSession(AuthState &state)
{
    if (strlen(HUB_AUTH_USERNAME) > 0 && strlen(HUB_AUTH_PASSWORD) > 0)
    {
        if (loginWithPassword(state))
        {
            Serial.printf("[auth] login_ok: %s\n", state.username.c_str());
            return true;
        }
        Serial.println("[auth] login_failed");
        return false;
    }

    if (createGuestSession(state))
    {
        Serial.printf("[auth] guest_ok: %s\n", state.username.c_str());
        return true;
    }

    Serial.println("[auth] anonymous_fallback");
    return false;
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
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting WiFi");
    unsigned long wifiStart = millis();
    const unsigned long wifiTimeoutMs = 15000;
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart) < wifiTimeoutMs)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("");
        Serial.print("WiFi OK: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("");
        Serial.println("WiFi connect timeout; will retry in loop.");
    }

    sio.onOpen([]()
               {
                StaticJsonDocument<128> d2;
                d2["room"] = IOT_ROOM;
                String s2; serializeJson(d2, s2);
                sio.emit("joinRoom", s2.c_str());

                StaticJsonDocument<64> d3;
                d3["observe"] = true;
                String s3; serializeJson(d3, s3);
                sio.emit("observeAllControl", s3.c_str());
                sio.emit("observeAllEvents", s3.c_str());

                onConnected(authState.username); });

    sio.on("control", onControlMessage);
    sio.on("event", onEventMessage);
    sio.on("chat", onChatMessage);

    establishInitialSession(authState);
    connectSocketWithAuth();
}

unsigned long lastSend = 0;

void loop()
{
    static unsigned long lastHeartbeat = 0;
    static unsigned long lastReconnectAttempt = 0;
    static unsigned long reconnectIntervalMs = 5000;
    static unsigned long lastWifiAttempt = 0;
    static unsigned long wifiRetryIntervalMs = 5000;
    static bool shouldReconnect = false;
    sio.loop();
    unsigned long now = millis();
    userScriptLoop();
    if (WiFi.status() != WL_CONNECTED)
    {
        if (now - lastWifiAttempt > wifiRetryIntervalMs)
        {
            Serial.println("[ESP32] WiFi disconnected, attempting reconnect...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
            lastWifiAttempt = now;
            if (wifiRetryIntervalMs < 60000UL)
            {
                wifiRetryIntervalMs = min(wifiRetryIntervalMs * 2, 60000UL);
            }
        }
        delay(100);
        return;
    }
    wifiRetryIntervalMs = 5000;
    if (!sio.connected())
    {
        if (!shouldReconnect)
        {
            Serial.println("[ESP32] Disconnected from server, will attempt reconnect...");
            shouldReconnect = true;
            lastReconnectAttempt = now;
        }
        if (shouldReconnect && (now - lastReconnectAttempt > reconnectIntervalMs))
        {
            Serial.println("[ESP32] Attempting reconnect...");
            bool hasSession = false;
            if (authState.refreshToken.length() > 0)
            {
                hasSession = refreshSession(authState);
                if (hasSession)
                {
                    Serial.printf("[auth] refresh_ok (%s): %s\n", authState.mode.c_str(), authState.username.c_str());
                }
            }
            if (!hasSession)
            {
                hasSession = establishInitialSession(authState);
            }
            connectSocketWithAuth();
            shouldReconnect = false;
            if (reconnectIntervalMs < 60000UL)
            {
                reconnectIntervalMs = min(reconnectIntervalMs * 2, 60000UL);
            }
        }
        delay(100);
        return;
    }
    else
    {
        shouldReconnect = false;
        reconnectIntervalMs = 5000;
    }
    if (now - lastHeartbeat > 2000)
    {
        // Serial.println("[ESP32] Loop heartbeat");
        lastHeartbeat = now;
    }
}
