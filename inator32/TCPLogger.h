#ifndef __TCPLOGGER_H
#define __TCPLOGGER_H

#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <WiFiServer.h>

class TCPLogger {
 public:
  TCPLogger();
  ~TCPLogger();

  void begin();
  void logmsg(String s);
  void logmsg(const char *msg);

  void loop();
  
 private:
  WiFiServer *tcpserver;
  WiFiClient tcpclient;
};

#endif
