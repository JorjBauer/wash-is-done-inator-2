#ifndef __WIFIMANAGER_H
#define __WIFIMANAGER_H

#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include "Prefs.h"

class WifiManager {
 public:
  WifiManager();
  ~WifiManager();

  void begin(const Prefs *p, const char *templateName);
  void loop();

  void JoinNetwork();

  int8_t getWifiPower();
 private:
  void StartSoftAP();
  
 private:
  const Prefs *myprefs;
  String baseName;
  
  bool softAP;
  bool scanUnderway;
  uint32_t nextWifiScan;
};

#endif
