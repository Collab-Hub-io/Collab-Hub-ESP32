#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <map>
#include <string>
#include "WsClient.h"

class SioClient
{
public:
  using TextHandler = std::function<void(const char *, size_t)>;
  using OpenHandler = std::function<void()>;

  SioClient();
  void begin(const char *host, uint16_t port, const char *nsp, bool useSSL, const char *username = nullptr);
  void loop();
  void emit(const char *event, const char *payloadJson);
  void on(const char *event, TextHandler handler);
  void onOpen(OpenHandler handler);
  bool connected();

private:
  void _handleText(const char *payload, size_t length);
  void _sendNamespaceOpen();
  void _sendPing();

  WsClient _ws;
  String _nsp;
  String _username;
  OpenHandler _openHandler = nullptr;
  std::map<std::string, TextHandler> _handlers;
  bool _open = false;
  uint32_t _pingIntervalMs = 0;
  uint32_t _lastPingMs = 0;
};
