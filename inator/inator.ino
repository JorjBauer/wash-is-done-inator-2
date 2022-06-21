#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WiFiServer.h>

#include "AudioFileSourceSPIFFS.h"
#include "lsm.h"
#include "debounce.h"

bool fsRunning = false;

LSM lsm;
Debounce sensor1, sensor2;

#define ALERTLED 1 // TX pin, GPIO 1. Blue.
#define SENSORLED 4 // D2 pin, GPIO 4. Green.

#define SENSOR1 16 // D0 pin, GPIO 16
#define SENSOR2 5 // D1 pin, GPIO 5

ESP8266WebServer server(80); //HTTP server on port 80
WiFiServer tcpserver(9001); // tcp server
WiFiClient tcpclient;

#define MAGIC 0x06212022
#define VERSION 1

typedef struct _prefs {
  uint32_t magic;
  uint8_t version;
  char ssid[50];
  char password[50];
} prefs;

prefs Prefs;
bool prefsOk = false;

void logmsg(const char *msg)
{
  if (tcpclient.connected()) {
    tcpclient.print(msg);
    tcpclient.flush();
  }
}

void handleReset()
{
  server.send(200, "text/html", "Okay, restarting");

  ESP.restart();
}

void handleStatus()
{
  String status = String("<html><div>SSID: ") +
    String(Prefs.ssid) +
    String("</div><div>Password: ") +
    String(Prefs.password) +
    String("</div></html>");
  server.send(200, "text/html", status.c_str());
}

void handleRoot()
{
  server.send(200, "text/html",
              "<!DOCTYPE html><html>"
              "<h1>Hola</h1>"
              "<p>Page list:</p><ul>"
              "<li><a href='/config'>/config</a>: change configuration (SSID, password)</li>"
              "<li><a href='/reset'>/reset</a>: reboots the device</li>"
              "<li><a href='/status'>/status</a>: show current status</li>"
              "</ul>"
              "<p>This also listens on port 9001 for debugging messages.</p>"
              "</html>"
              );
}

void handleConfig()
{
  String html =
    String("<!DOCTYPE html><html>"
           "<head></head>"
           "<body>"
           "<form action='/submit' method='post'>"
           "<div><label for='ssid'>Connect to SSID:</label>"
           "<input type='text' id='ssid' name='ssid' value='") +
    String(Prefs.ssid) +
    String("'/></div>"
           "<div><label for='password' id='password' name='password' value='") +
    String(Prefs.password) +
    String("'/></div>"
           "<div><input type='submit' value='Save' /></div>"
           "</body></html");
  server.send(200, "text/html",
              html.c_str());
}

void handleSubmit()
{
  String new_ssid = server.arg("ssid");
  String new_password = server.arg("password");
  Prefs.magic = MAGIC;
  Prefs.version = VERSION;
  strncpy(Prefs.ssid, new_ssid.c_str(), sizeof(Prefs.ssid));
  strncpy(Prefs.password, new_password.c_str(), sizeof(Prefs.password));
  
  writePrefs();

  // Redirect to /status to show the changes
  server.sendHeader("Location", String("/status"), true);
  server.send(302, "text/plain", "");
}

void StartSoftAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("WashIsDoneInator");
  // Set the IP address and info for SoftAP mode. Note this is also
  // the default IP (192.168.4.1), but better to be explicit...
  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
}


void setup()
{
  // Can't use the serial port for debugging - it's using the TX/RX pins
  // to pass through to a USB chip. Also: don't leave the Arduino serial
  // monitor running for the same reasons...
  delay(1000); // let power settle
  if (SPIFFS.begin()) {
    fsRunning = true;
  } else {
    fsRunning = false;
  }

  if (fsRunning) {
    // Try to load the config file
    fs::File f = SPIFFS.open("/inator.cfg", "r");
    if (!f) {
      // No config file found; create a set of defaults and save it
      f = SPIFFS.open("/inator.cfg", "w");
      Prefs.magic = MAGIC;
      Prefs.version = VERSION;
      strncpy(Prefs.ssid, "", sizeof(Prefs.ssid));
      strncpy(Prefs.password, "", sizeof(Prefs.password));
      f.close();
      f = SPIFFS.open("/inator.cfg", "r");
      if (f) {
        fsRunning = true;
        prefsOk = true;
      }
    }
    
    if (f) {
      prefsOk = readPrefs(f);
    }
  }

  // Turn TX/RX in to GPIO1 and GPIO 3
  pinMode(1, FUNCTION_3);
  pinMode(3, FUNCTION_3);

  pinMode(ALERTLED, OUTPUT);
  pinMode(SENSORLED, OUTPUT);

  pinMode(SENSOR1, INPUT);
  pinMode(SENSOR2, INPUT);

  pinMode(A0, INPUT);

  if (!prefsOk ||
      (prefsOk && !strlen(Prefs.ssid))) {
    StartSoftAP();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(Prefs.ssid, Prefs.password);
    uint8_t count=0;
    while (count < 10 && WiFi.waitForConnectResult() != WL_CONNECTED) {
      count++;
      delay(5000);
    }
    if (count >= 10) {
      while (count--) {
        digitalWrite(ALERTLED, HIGH);
        digitalWrite(SENSORLED, LOW);
        delay(1000);
        digitalWrite(ALERTLED, LOW);
        digitalWrite(SENSORLED, HIGH);
        delay(1000);
      }
      StartSoftAP();
    }
  }
  
  ArduinoOTA.setHostname("doneinator");
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/submit", handleSubmit);
  server.on("/reset", handleReset);
  server.on("/status", handleStatus);
  
  server.begin();
  tcpserver.begin();
}

bool buttonIsPressed()
{
  return (analogRead(A0) < (1023/2));
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  if (tcpserver.hasClient()) {
    tcpclient = tcpserver.available();
    tcpclient.println("Hello");
    tcpclient.flush();
  }

  // Track the alerting state with the alert LED
  if (lsm.isAlerting()) {
    digitalWrite(ALERTLED, HIGH);
  } else {
    digitalWrite(ALERTLED, LOW);
  }

  // If we're delaying before re-alterting or playing the tune, and
  // the button's pressed, then abort; that's confirmation that alert
  // was received
  if (lsm.isAlerting() && buttonIsPressed()) {
    lsm.buttonPressed();
    // Shut off the LEDs and delay a short time so we don't accidentally
    // trigger the sensor on our own LEDs
    digitalWrite(ALERTLED, LOW);
    digitalWrite(SENSORLED, LOW);
    delay(200);
    return; // abandon this run - do not update lsm's sensor readings
  }

  bool s1 = digitalRead(SENSOR1);
  sensor1.input(s1);
  sensor2.input(digitalRead(SENSOR2));
  bool sensorState = !sensor1.output();

  static bool prevs1;
  if (s1 != prevs1) {
    if (s1) {
      logmsg("s1 is TRUE\n");
    } else {
      logmsg("s1 is false\n");
    }
    prevs1 = s1;
  }
  static bool prevss;
  if (sensorState != prevss) {
    if (sensorState) {
      logmsg("sensorState is TRUE\n");
    } else {
      logmsg("sensorState is false\n");
    }
    prevss = sensorState;
  }

  digitalWrite(SENSORLED, sensorState);
  if (lsm.sensorState(sensorState)) {
    // If the state machine changes states... then transmit an update? hmm. No.
  }

}

void writePrefs()
{
  fs::File f = SPIFFS.open("/inator.cfg", "w");
  f.write((unsigned char *)(&Prefs), sizeof(Prefs));
  f.close();
}

bool readPrefs(fs::File f)
{
  if (f.size() != sizeof(Prefs)) {
    return false;
  }

  unsigned char *ptr = (unsigned char *)(&Prefs);
  while (f.available()) {
    *ptr++ = f.read();
  }

  if (Prefs.magic != MAGIC)
    return false;

  if (Prefs.version != VERSION)
    return false;
  
  return true;
}
