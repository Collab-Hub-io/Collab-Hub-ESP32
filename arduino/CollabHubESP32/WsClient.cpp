#include "WsClient.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>

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
  _client.setInsecure(); // For testing only; remove for production and use CA cert
  if (!_client.connect(host, port))
    return false;
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
    _client.write(req[i]);
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
    while (_client.available())
    {
      char c = _client.read();
      statusLine += c;
      if (statusLine.endsWith("\r\n\r\n"))
      {
        // Serial.println("[WsClient] --- HTTP handshake response ---");
        // Serial.print(statusLine);
        // Serial.println("[WsClient] --- END HTTP handshake response ---");
        _handshook = statusLine.startsWith("HTTP/1.1 101");
        if (!_handshook)
        {
          // Serial.println("WebSocket handshake failed:");
          // Serial.println(statusLine);
        }
        else
        {
          // Serial.println("WebSocket handshake OK");
          // Serial.println("[WsClient] WebSocket connection established, starting frame reader");
        }
        return _handshook;
      }
    }
    delay(10);
  }
  // Serial.println("WebSocket handshake timeout");
  return false;
}

// Minimal frame reader: text frames only, unmasked (server->client)
bool WsClient::_readFrame(String &out)
{
  if (!_client.connected())
  {
    return false;
  }
  if (!_client.available())
  {
    return false;
  }
  uint8_t hdr1 = _client.read();
  // Serial.print("[WsClient] Incoming frame: hdr1=0x");
  // Serial.println(hdr1, HEX);
  if ((hdr1 & 0x0F) == 0x9)
  {
    // Serial.println("[WsClient] Received PING frame (opcode 0x9)");
    uint8_t hdr2 = _client.read();
    size_t len = hdr2 & 0x7F;
    // Serial.print("[WsClient] Ping payload length: ");
    // Serial.println(len);
    for (size_t i = 0; i < len; i++)
    {
      uint8_t b = _client.read();
      // Serial.print("[WsClient] Ping payload byte: ");
      // Serial.println(b, HEX);
    }
    // Send pong
    uint8_t pong[2] = {0x8A, 0x00};
    _client.write(pong, 2);
    // Serial.println("[WsClient] Sent PONG frame (opcode 0xA)");
    return false;
  }
  if ((hdr1 & 0x0F) != 0x1)
  {
    // Serial.print("[WsClient] Non-text frame received, opcode: 0x");
    // Serial.println(hdr1 & 0x0F, HEX);
    uint8_t hdr2 = _client.read();
    size_t len = hdr2 & 0x7F;
    // Serial.print("[WsClient] Skipping payload length: ");
    // Serial.println(len);
    for (size_t i = 0; i < len; i++)
    {
      uint8_t b = _client.read();
      // Serial.print("[WsClient] Skipped byte: ");
      // Serial.println(b, HEX);
    }
    return false;
  }
  uint8_t hdr2 = _client.read();
  bool masked = hdr2 & 0x80;
  size_t len = hdr2 & 0x7F;
  if (len == 126)
  {
    uint8_t b1 = _client.read();
    uint8_t b2 = _client.read();
    len = ((size_t)b1 << 8) | b2;
  }
  else if (len == 127)
  {
    // Serial.println("[WsClient] Frame too long, aborting");
    return false;
  }
  uint8_t mask[4] = {0, 0, 0, 0};
  if (masked)
  {
    for (int i = 0; i < 4; i++)
      mask[i] = _client.read();
  }
  out.reserve(len);
  // Serial.print("[WsClient] Text frame payload length: ");
  // Serial.println(len);
  // Serial.print("[WsClient] Masked: ");
  // Serial.println(masked);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t b = _client.read();
    if (masked)
      b ^= mask[i % 4];
    out += (char)b;
  }
  // Serial.print("[WsClient] Text frame payload: ");
  // Serial.println(out);
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
  if (!_client.connected())
    return false;
  // Client frames must be masked
  uint8_t hdr1 = 0x81; // FIN + text
  _client.write(&hdr1, 1);
  uint8_t mask[4];
  for (int i = 0; i < 4; i++)
    mask[i] = (uint8_t)random(0, 256);
  if (len < 126)
  {
    uint8_t hdr2 = 0x80 | (uint8_t)len;
    _client.write(&hdr2, 1);
  }
  else if (len < 65536)
  {
    uint8_t hdr2 = 0xFE; // 126 + mask bit
    _client.write(&hdr2, 1);
    uint8_t ext[2] = {(uint8_t)(len >> 8), (uint8_t)(len & 0xFF)};
    _client.write(ext, 2);
  }
  else
  {
    return false; // too large
  }
  _client.write(mask, 4);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t b = data[i] ^ mask[i % 4];
    _client.write(&b, 1);
  }
  return true;
}
