#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <functional>

class WsClient
{
public:
  using MessageHandler = std::function<void(const char *data, size_t len)>;

  WsClient() {}
  bool connect(const char *host, uint16_t port, const char *path);
  void poll(MessageHandler onMessage);
  bool sendText(const char *data, size_t len);
  bool connected() { return _client.connected(); }

private:
  WiFiClientSecure _client;
  bool _handshook = false;
  String _host;
  uint16_t _port = 0;
  String _path;

  String _genKey();
  bool _readHttpResponse();
  bool _readFrame(String &out);
};
