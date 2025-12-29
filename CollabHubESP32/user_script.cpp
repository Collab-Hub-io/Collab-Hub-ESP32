
#include "SioClient.h"
#include "user_script.h"
#include <ArduinoJson.h>
extern SioClient sio;

// ================= USER MESSAGE EMITTERS (Don't Edit)=================

void emitControl(const char *header, float value, const char *mode, const char *target)
{
    StaticJsonDocument<256> doc;
    doc["header"] = header;
    doc["values"] = value;
    doc["mode"] = mode;
    doc["target"] = target;
    String s;
    serializeJson(doc, s);
    sio.emit("control", s.c_str());
}

void emitEvent(const char *header, const char *payload)
{
    StaticJsonDocument<256> doc;
    doc["header"] = header;
    doc["mode"] = "push";
    doc["target"] = "all";
    if (payload && strlen(payload) > 0)
    {
        doc["payload"] = payload;
    }
    String s;
    serializeJson(doc, s);
    sio.emit("event", s.c_str());
}

void emitChat(const char *text)
{
    StaticJsonDocument<128> doc;
    doc["chat"] = text;
    doc["mode"] = "push";
    doc["target"] = "all";
    String s;
    serializeJson(doc, s);
    sio.emit("chat", s.c_str());
}

// ================= USER DEFINED VARIABLES (Edit as needed)=================

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

// Control pin definitions (change as needed)
// Current example uses GPIO ping to trigger event, control, and chat messages

#define EVENT_PIN 0   // Change to desired GPIO pin
#define CTRL_PIN_0 4  // Change to desired GPIO pin
#define CTRL_PIN_1 16 // Change to desired GPIO pin
#define CTRL_PIN_2 17 // Change to desired GPIO pin
// Control button pins
static bool lastEventPinState = HIGH;
static bool lastCtrl0State = HIGH;
static bool lastCtrl1State = HIGH;
static bool lastCtrl2State = HIGH;

// ================= User Defined Methods (Add and Edit)=================
// (Add any additional user-defined methods here)
// Example: Simple counter increment/decrement methods

static int counter = 0;
static int previousControlValue = -1;

void incrementCounter()
{
    counter++;
    if (counter > 127)
        counter = 127;

    if (counter != previousControlValue)
    {
        Serial.print("[user_script] Counter incremented to: ");
        Serial.println(counter);
        previousControlValue = counter;
        emitControl("espControl", static_cast<float>(counter));
    }
}

void decrementCounter()
{
    counter--;
    if (counter < 0)
        counter = 0;
    if (counter != previousControlValue)
    {
        Serial.print("[user_script] Counter decremented to: ");
        Serial.println(counter);
        previousControlValue = counter;
        emitControl("espControl", static_cast<float>(counter));
    }
}

// ================= USER SCRIPT HOOKS (Edit as needed)=================

// Called once at startup from CollabHubESP32.ino setup()
void userScriptSetup()
{
    pinMode(EVENT_PIN, INPUT_PULLUP);
    pinMode(CTRL_PIN_0, INPUT_PULLUP);
    pinMode(CTRL_PIN_1, INPUT_PULLUP);
    pinMode(CTRL_PIN_2, INPUT_PULLUP);
    lastEventPinState = digitalRead(EVENT_PIN);
}

// Called repeatedly from CollabHubESP32.ino loop()
void userScriptLoop()
{
    // Check event pin state for falling edge
    bool eventPinState = digitalRead(EVENT_PIN);
    bool ctrl0State = digitalRead(CTRL_PIN_0);
    bool ctrl1State = digitalRead(CTRL_PIN_1);
    bool ctrl2State = digitalRead(CTRL_PIN_2);
    // control buttons (active low)
    if (ctrl0State == LOW)
    {
        decrementCounter();
    }
    if (ctrl1State == LOW)
    {
        incrementCounter();
    }
    if (lastCtrl2State == HIGH && ctrl2State == LOW)
    {
        // emit chat message
        emitChat("ESP32 says hello!");
    }
    // event pin falling edge detection
    if (lastEventPinState == HIGH && eventPinState == LOW)
    {
        onEventPinPressed();
    }
    // clean up states
    lastEventPinState = eventPinState;
    lastCtrl0State = ctrl0State;
    lastCtrl1State = ctrl1State;
    lastCtrl2State = ctrl2State;
}

// Example event pin handler
void onEventPinPressed()
{
    // Example: Blink LED on event pin press
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("[user_script] Event pin pressed!");
    emitEvent("espEvent");
}

// Example message handlers -- called upon receiving control messages from CollabHubESP32.ino
void onControlMessage(const char *json, size_t len)
{
    // Example: Print control message
    Serial.print("[user_script] Control: ");
    Serial.write(json, len);
    Serial.println();
}

// Example message handlers -- called upon receiving event messages from CollabHubESP32.ino
void onEventMessage(const char *json, size_t len)
{
    // Example: Print event message
    Serial.print("[user_script] Event: ");
    Serial.write(json, len);
    Serial.println();
}

// Example message handlers -- called upon receiving chat messages from CollabHubESP32.ino
void onChatMessage(const char *json, size_t len)
{
    // Example: Print chat message
    Serial.print("[user_script] Chat: ");
    Serial.write(json, len);
    Serial.println();
}

// Example connected handler -- called once upon successful connection from CollabHubESP32.ino
void onConnected(const String &username)
{
    // Example: Print username on connect
    Serial.print("[user_script] Connected as: ");
    Serial.println(username);
}
