#include "SioClient.h"
#include <ArduinoJson.h>
#include <cstring>
#include <string>

bool SioClient::connected()
{
  return _ws.connected();
}
#include <ArduinoJson.h>
#include <cstring>
#include <string>

SioClient::SioClient() {}

void SioClient::begin(const char *host, uint16_t port, const char *nsp, bool useSSL, const char *username)
{
  _nsp = (nsp && *nsp) ? String(nsp) : String("/");
  _username = (username && *username) ? String(username) : String("");
  String path = "/socket.io/?EIO=4&transport=websocket";
  if (_username.length() > 0)
  {
    path += "&username=";
    path += _username;
  }
  // Serial.print("WS connecting to ");
  // Serial.print(host);
  // Serial.print(":");
  // Serial.print(port);
  // Serial.print(" path ");
  // Serial.println(path);
  bool ok = _ws.connect(host, port, path.c_str());
  if (!ok)
  {
    // Serial.println("WS connect failed (TCP)");
  }
}

void SioClient::loop()
{
  _ws.poll([this](const char *data, size_t len)
           { _handleText(data, len); });

  uint32_t now = millis();
  // Print current connection status and ping interval
  static bool lastConnected = true;
  bool currentConnected = connected();
  if (lastConnected != currentConnected)
  {
    // Serial.print("[SioClient] Connection status changed: ");
    // Serial.println(currentConnected ? "CONNECTED" : "DISCONNECTED");
    lastConnected = currentConnected;
  }
  // ...existing code...

  // Do NOT send client-initiated pings. Only respond to server pings.
}

void SioClient::emit(const char *event, const char *payloadJson)
{
  String frame = "42";
  if (_nsp.length() > 1)
  {
    frame += _nsp;
    frame += ",";
  }
  frame += "[\"";
  frame += event;
  frame += "\",";
  frame += (payloadJson && *payloadJson) ? payloadJson : "{}";
  frame += "]";
  _ws.sendText(frame.c_str(), frame.length());
}

void SioClient::on(const char *event, TextHandler handler)
{
  _handlers[std::string(event)] = handler;
}

void SioClient::onOpen(OpenHandler handler)
{
  _openHandler = handler;
}

void SioClient::_handleText(const char *payload, size_t length)
{
  // Serial.print("[SioClient] _handleText received: ");
  // for (size_t i = 0; i < length; ++i)
  //   Serial.print(payload[i]);
  // Serial.print(" (len=");
  // Serial.print(length);
  // Serial.println(")");
  if (length == 0)
    return;
  if (payload[0] == '0')
  {
    // Serial.println("EIO open frame received");
    StaticJsonDocument<256> doc;
    auto err = deserializeJson(doc, payload + 1, length - 1);
    if (!err)
    {
      _pingIntervalMs = doc["pingInterval"].as<uint32_t>();
      // Serial.print("pingIntervalMs=");
      // Serial.println(_pingIntervalMs);
    }
    _sendNamespaceOpen();
    return;
  }
  if (payload[0] == '2')
  { // server ping
    // Serial.println("[SioClient] Server ping (2) received");
    // Serial.println("[SioClient] Sending pong (3)");
    _ws.sendText("3", 1);   // pong
    _lastPingMs = millis(); // update last ping time
    return;
  }
  if (payload[0] == '3')
  {
    // Serial.println("[SioClient] Pong (3) received from server");
    return;
  }
  if (payload[0] == '4' && length >= 2 && payload[1] == '0')
  {
    _open = true;
    // Serial.println("Namespace open ack (40) received");
    if (_openHandler)
      _openHandler();
    return;
  }
  if (payload[0] == '4' && length >= 2 && payload[1] == '2')
  {
    const char *start = payload + 2;
    if (*start == '/')
    {
      const char *comma = strchr(start, ',');
      if (!comma)
        return;
      start = comma + 1;
    }
    StaticJsonDocument<512> arr;
    auto err = deserializeJson(arr, start);
    if (err)
      return;
    const char *evt = arr[0].as<const char *>();
    // Serial.print("Event frame received: ");
    // Serial.println(evt);
    String payloadStr;
    if (!arr[1].isNull())
    {
      StaticJsonDocument<512> tmp;
      tmp.set(arr[1]);
      serializeJson(tmp, payloadStr);
    }
    else
    {
      payloadStr = "{}";
    }
    auto it = _handlers.find(std::string(evt));
    if (it != _handlers.end())
    {
      it->second(payloadStr.c_str(), payloadStr.length());
    }
    return;
  }
}

void SioClient::_sendNamespaceOpen()
{
  String frame = "40";
  if (_nsp.length() > 1)
    frame += _nsp;
  _ws.sendText(frame.c_str(), frame.length());
}

void SioClient::_sendPing()
{
  // Serial.println("[SioClient] Sending ping (2) to server");
  _ws.sendText("2", 1);
}
