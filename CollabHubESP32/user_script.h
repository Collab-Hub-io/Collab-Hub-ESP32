
#pragma once
#include <Arduino.h>

// ================= USER SCRIPT HOOKS =================

/**
 * @brief Called once during setup() for user initialization (e.g., pinMode).
 */
void userScriptSetup();

/**
 * @brief Called every loop() for user polling (e.g., GPIO, sensors).
 */
void userScriptLoop();

/**
 * @brief Called when a control message is received from the server.
 * @param json Raw JSON string
 * @param len Length of JSON string
 */
void onControlMessage(const char *json, size_t len);

/**
 * @brief Called when an event message is received from the server.
 * @param json Raw JSON string
 * @param len Length of JSON string
 */
void onEventMessage(const char *json, size_t len);

/**
 * @brief Called when a chat message is received from the server.
 * @param json Raw JSON string
 * @param len Length of JSON string
 */
void onChatMessage(const char *json, size_t len);

/**
 * @brief Called when the ESP32 joins the namespace/room.
 * @param username The generated username for this device
 */
void onConnected(const String &username);

/**
 * @brief Called when the event pin is pressed (falling edge).
 */
void onEventPinPressed();

// ================= USER MESSAGE EMITTERS =================

/**
 * @brief Emit a control message to the server.
 * @param header Control header string
 * @param value Control value (float)
 * @param mode Control mode (default: "push")
 * @param target Target recipient (default: "all")
 */
void emitControl(const char *header, float value, const char *mode = "push", const char *target = "all");

/**
 * @brief Emit an event message to the server.
 * @param header Event header string
 * @param payload Optional payload string (default: nullptr)
 */
void emitEvent(const char *header, const char *payload = nullptr);

/**
 * @brief Emit a chat message to the server.
 * @param text Chat message text
 */
void emitChat(const char *text);
