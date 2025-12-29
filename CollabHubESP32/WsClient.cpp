
#include "WsClient.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>

#if USE_TLS
bool WsClient::connected()
{
  return _clientSecure.connected();
}
#else
bool WsClient::connected()
{
  return _clientPlain.connected();
}
#endif

static String _base64Encode(const uint8_t *data, size_t len)
{
  static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while (i < len)
  {
    uint32_t chunk = 0;
    int bytes = 0;
    for (int j = 0; j < 3; ++j)
    {
      if (i < len)
      {
        chunk = (chunk << 8) | data[i++];
        bytes++;
      }
      else
      {
        chunk <<= 8;
      }
    }
    uint8_t b0 = (chunk >> 18) & 0x3F;
    uint8_t b1 = (chunk >> 12) & 0x3F;
    uint8_t b2 = (chunk >> 6) & 0x3F;
    uint8_t b3 = chunk & 0x3F;
    out += tbl[b0];
    out += tbl[b1];
    out += (bytes > 1) ? tbl[b2] : '=';
    out += (bytes > 2) ? tbl[b3] : '=';
  }
  return out;
}

String WsClient::_genKey()
{
  uint8_t key[16];
  for (int i = 0; i < 16; ++i)
    key[i] = (uint8_t)random(0, 256);
  return _base64Encode(key, 16);
}

bool WsClient::connect(const char *host, uint16_t port, const char *path)
{
  _host = host;
  _port = port;
  _path = path;
#if USE_TLS
  _clientSecure.setInsecure(); // For testing only; remove for production and use CA cert
  if (!_clientSecure.connect(host, port))
    return false;
#else
  if (!_clientPlain.connect(host, port))
    return false;
#endif
  String key = _genKey();
  String req;
  req += "GET ";
  req += path;
  req += " HTTP/1.1\r\n";
  req += "Host: ";
  req += host;
  req += ":";
  req += String(port);
  req += "\r\n";
  req += "Upgrade: websocket\r\n";
  req += "Connection: Upgrade\r\n";
  req += "Sec-WebSocket-Version: 13\r\n";
  req += "Sec-WebSocket-Key: ";
  req += key;
  req += "\r\n";
  req += "Origin: http://";
  req += host;
  req += "\r\n";
  req += "\r\n";
  // Serial.println("[WsClient] --- HTTP handshake request ---");
  // Serial.print(req);
  // Serial.println("[WsClient] --- END HTTP handshake request ---");
  for (size_t i = 0; i < req.length(); ++i)
  {
#if USE_TLS
    _clientSecure.write(req[i]);
#else
    _clientPlain.write(req[i]);
#endif
  }
  // Serial.println("[WsClient] Sent handshake request, waiting for response...");
  return _readHttpResponse();
}

bool WsClient::_readHttpResponse()
{
  unsigned long start = millis();
  String statusLine;
  // Serial.println("[WsClient] Waiting for HTTP handshake response...");
  while (millis() - start < 5000)
  {
#if USE_TLS
    while (_clientSecure.available())
    {
      char c = _clientSecure.read();
      statusLine += c;
      if (statusLine.endsWith("\r\n\r\n"))
      {
        _handshook = statusLine.startsWith("HTTP/1.1 101");
        return _handshook;
      }
    }
#else
    while (_clientPlain.available())
    {
      char c = _clientPlain.read();
      statusLine += c;
      if (statusLine.endsWith("\r\n\r\n"))
      {
        _handshook = statusLine.startsWith("HTTP/1.1 101");
        return _handshook;
      }
    }
#endif
    delay(10);
  }
  // Serial.println("WebSocket handshake timeout");
  return false;
}

// Minimal frame reader: text frames only, unmasked (server->client)
bool WsClient::_readFrame(String &out)
{
#if USE_TLS
  if (!_clientSecure.connected())
    return false;
  if (!_clientSecure.available())
    return false;
  uint8_t hdr1 = _clientSecure.read();
#else
  if (!_clientPlain.connected())
    return false;
  if (!_clientPlain.available())
    return false;
  uint8_t hdr1 = _clientPlain.read();
#endif
  // Serial.print("[WsClient] Incoming frame: hdr1=0x");
  // Serial.println(hdr1, HEX);
  if ((hdr1 & 0x0F) == 0x9)
  {
#if USE_TLS
    uint8_t hdr2 = _clientSecure.read();
    size_t len = hdr2 & 0x7F;
    for (size_t i = 0; i < len; i++)
      _clientSecure.read();
    uint8_t pong[2] = {0x8A, 0x00};
    _clientSecure.write(pong, 2);
#else
    uint8_t hdr2 = _clientPlain.read();
    size_t len = hdr2 & 0x7F;
    for (size_t i = 0; i < len; i++)
      _clientPlain.read();
    uint8_t pong[2] = {0x8A, 0x00};
    _clientPlain.write(pong, 2);
#endif
    return false;
  }
  if ((hdr1 & 0x0F) != 0x1)
  {
#if USE_TLS
    uint8_t hdr2 = _clientSecure.read();
    size_t len = hdr2 & 0x7F;
    for (size_t i = 0; i < len; i++)
      _clientSecure.read();
#else
    uint8_t hdr2 = _clientPlain.read();
    size_t len = hdr2 & 0x7F;
    for (size_t i = 0; i < len; i++)
      _clientPlain.read();
#endif
    return false;
  }
#if USE_TLS
  uint8_t hdr2 = _clientSecure.read();
  bool masked = hdr2 & 0x80;
  size_t len = hdr2 & 0x7F;
  if (len == 126)
  {
    uint8_t b1 = _clientSecure.read();
    uint8_t b2 = _clientSecure.read();
    len = ((size_t)b1 << 8) | b2;
  }
  else if (len == 127)
  {
    return false;
  }
  uint8_t mask[4] = {0, 0, 0, 0};
  if (masked)
  {
    for (int i = 0; i < 4; i++)
      mask[i] = _clientSecure.read();
  }
  out.reserve(len);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t b = _clientSecure.read();
    if (masked)
      b ^= mask[i % 4];
    out += (char)b;
  }
#else
  uint8_t hdr2 = _clientPlain.read();
  bool masked = hdr2 & 0x80;
  size_t len = hdr2 & 0x7F;
  if (len == 126)
  {
    uint8_t b1 = _clientPlain.read();
    uint8_t b2 = _clientPlain.read();
    len = ((size_t)b1 << 8) | b2;
  }
  else if (len == 127)
  {
    return false;
  }
  uint8_t mask[4] = {0, 0, 0, 0};
  if (masked)
  {
    for (int i = 0; i < 4; i++)
      mask[i] = _clientPlain.read();
  }
  out.reserve(len);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t b = _clientPlain.read();
    if (masked)
      b ^= mask[i % 4];
    out += (char)b;
  }
#endif
  return true;
}

void WsClient::poll(MessageHandler onMessage)
{
  String msg;
  if (_readFrame(msg))
  {
    onMessage(msg.c_str(), msg.length());
  }
}

bool WsClient::sendText(const char *data, size_t len)
{
#if USE_TLS
  if (!_clientSecure.connected())
    return false;
  uint8_t hdr1 = 0x81; // FIN + text
  _clientSecure.write(&hdr1, 1);
  uint8_t mask[4];
  for (int i = 0; i < 4; i++)
    mask[i] = (uint8_t)random(0, 256);
  if (len < 126)
  {
    uint8_t hdr2 = 0x80 | (uint8_t)len;
    _clientSecure.write(&hdr2, 1);
  }
  else if (len < 65536)
  {
    uint8_t hdr2 = 0xFE;
    _clientSecure.write(&hdr2, 1);
    uint8_t ext[2] = {(uint8_t)(len >> 8), (uint8_t)(len & 0xFF)};
    _clientSecure.write(ext, 2);
  }
  else
  {
    return false;
  }
  _clientSecure.write(mask, 4);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t b = data[i] ^ mask[i % 4];
    _clientSecure.write(&b, 1);
  }
  return true;
#else
  if (!_clientPlain.connected())
    return false;
  uint8_t hdr1 = 0x81; // FIN + text
  _clientPlain.write(&hdr1, 1);
  uint8_t mask[4];
  for (int i = 0; i < 4; i++)
    mask[i] = (uint8_t)random(0, 256);
  if (len < 126)
  {
    uint8_t hdr2 = 0x80 | (uint8_t)len;
    _clientPlain.write(&hdr2, 1);
  }
  else if (len < 65536)
  {
    uint8_t hdr2 = 0xFE;
    _clientPlain.write(&hdr2, 1);
    uint8_t ext[2] = {(uint8_t)(len >> 8), (uint8_t)(len & 0xFF)};
    _clientPlain.write(ext, 2);
  }
  else
  {
    return false;
  }
  _clientPlain.write(mask, 4);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t b = data[i] ^ mask[i % 4];
    _clientPlain.write(&b, 1);
  }
  return true;
#endif
}
