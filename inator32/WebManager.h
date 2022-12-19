#ifndef __WEBMANAGER_H
#define __WEBMANAGER_H

#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WebServer.h>
#else
#include <WebServer.h>
#endif
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "Prefs.h"
#ifdef ESP32
#include "SPIFFS.h"
#endif

#include "WifiManager.h"

class WebManager : virtual public
#ifdef ESP8266
  ESP8266WebServer
#else
  WebServer
#endif
{
 public:
  WebManager(int port);
  ~WebManager();

  void begin(const Prefs *p);
  void loop();

  bool isAuthenticated(String fromWhere = "/");

  void sendFileHandle(fs::File f);

  void SendHeader();
  void SendFooter();

  static void handleIndex();
  static void handleLoginGet();
  static void handleLoginPost();
  static void handleStatus();
  static void handleConfig();
  static void handleSubmit();
  static void handleUpload();
  static void handleDownload();
  static void handleRm();
  static void handleLs();
  static void handleRestart();

public:
  volatile uint32_t epochTime;
  uint32_t lastTimeUpdate;
private:
  WiFiUDP ntpUdp;
  NTPClient *timeClient;
  uint32_t nextTimeUpdate;
};

#endif
