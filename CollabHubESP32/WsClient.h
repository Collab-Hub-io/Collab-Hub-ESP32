#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include "config.h"
#include <functional>

class WsClient
{
public:
  using MessageHandler = std::function<void(const char *data, size_t len)>;

  WsClient() {}
  bool connect(const char *host, uint16_t port, const char *path);
  void poll(MessageHandler onMessage);
  bool sendText(const char *data, size_t len);
  bool connected();
  void disconnect();

private:
#if USE_TLS
  WiFiClientSecure _clientSecure;
#else
  WiFiClient _clientPlain;
#endif
  static const size_t kMaxFrameSize = 2048;
  enum FrameStage
  {
    StageHeader,
    StageExtLen,
    StageMask,
    StagePayload
  };

  bool _handshook = false;
  String _host;
  uint16_t _port = 0;
  String _path;

  String _genKey();
  bool _readHttpResponse();
  bool _readFrame(String &out);
  void _resetFrameState();

  FrameStage _stage = StageHeader;
  uint8_t _hdr1 = 0;
  uint8_t _hdr2 = 0;
  uint8_t _opcode = 0;
  bool _masked = false;
  uint8_t _mask[4] = {0, 0, 0, 0};
  uint8_t _maskIndex = 0;
  uint8_t _extLen[8] = {0};
  size_t _extLenNeeded = 0;
  size_t _extLenRead = 0;
  size_t _payloadLen = 0;
  size_t _payloadRead = 0;
  bool _dropFrame = false;
  bool _shouldClose = false;
  String _frameBuffer;
};
