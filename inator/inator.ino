#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WiFiServer.h>
#include <arduino_homekit_server.h>

#include "AudioFileSourceSPIFFS.h"
#include "lsm.h"
#include "debounce.h"

bool fsRunning = false;
bool homekit_initialized = false;

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
  float volume;
} prefs;

prefs Prefs;
bool prefsOk = false;

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t leakDetected;

void logmsg(const char *msg)
{
  if (tcpclient.connected()) {
    tcpclient.println(msg);
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
    String("</div><div>Volume: ") +
    String(Prefs.volume) +
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
              "<li><a href='/trigger'>/trigger</a>: trigger alert now</li>"
              "<li><a href='/stop'>/stop</a>: simulate pressing the button to stop alert</li>"
              "</ul>"
              "<p>This also listens on port 9001 for debugging messages.</p>"
              "</html>"
              );
}

void handleTrigger()
{
  lsm.trigger();
  server.send(200, "text/html", "Okay, triggered");
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
           "<div><label for='password'>Network Password:</label>"
           "<input type='password' id='password' name='password' value='") +
    String(Prefs.password) +
    String("'/></div>"
           "<div><label for='volume'>Volume (0.0-1.0):</label>"
           "<input type='number' id='volume' name='volume' step='0.01' value='") +
    String(Prefs.volume) +
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
  String new_volume = server.arg("volume");
  Prefs.magic = MAGIC;
  Prefs.version = VERSION;
  strncpy(Prefs.ssid, new_ssid.c_str(), sizeof(Prefs.ssid));
  strncpy(Prefs.password, new_password.c_str(), sizeof(Prefs.password));
  Prefs.volume = atof(new_volume.c_str());
  lsm.setVolume(Prefs.volume);
  
  writePrefs();

  // Redirect to /status to show the changes
  server.sendHeader("Location", String("/status"), true);
  server.send(302, "text/plain", "");
}

void handleStop()
{
  lsm.buttonPressed();
  server.send(200, "text/html", "Stopping");
}

void handleDebug()
{
  server.send(200, "text/html", "Ok starting up homekit stuff");
  // This would go in setup I think

  arduino_homekit_setup(&config);
  homekit_initialized = true;
  logmsg("started homekit");
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

  // Set some default preferences in case we can't load any prefs...
  strncpy(Prefs.ssid, "", sizeof(Prefs.ssid));
  strncpy(Prefs.password, "", sizeof(Prefs.password));
  Prefs.volume = 0.05; // Default to a low but audible volume
  lsm.setVolume(Prefs.volume);

  if (fsRunning) {
    // Try to load the config file
    fs::File f = SPIFFS.open("/inator.cfg", "r");
    if (!f) {
      // No config file found; create a set of defaults and save it
      f = SPIFFS.open("/inator.cfg", "w");
      Prefs.magic = MAGIC;
      Prefs.version = VERSION;
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
  server.on("/trigger", handleTrigger);
  server.on("/stop", handleStop);
  server.on("/debug", handleDebug);
  
  server.begin();
  tcpserver.begin();
}

bool buttonIsPressed()
{
  return (analogRead(A0) < (1023/2));
}

void my_homekit_loop()
{
  arduino_homekit_loop();
  static uint32_t nextReport = 0;
  static bool ld = false;
  if (millis() > nextReport) {
    leakDetected.value.int_value = (ld ? 1 : 0);
    homekit_characteristic_notify(&leakDetected, leakDetected.value);
    ld = !ld;
    nextReport = millis() + 60000; // 60 seconds
  }
}


void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  
  if (homekit_initialized) {
    my_homekit_loop();
    return; // debugging... why can't I pair?
  } 
  

  if (tcpserver.hasClient()) {
    if (tcpclient.connected()) {
      tcpclient.stop(); // only one debugging session at a time please
    }
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

  sensor1.input(digitalRead(SENSOR1));
  sensor2.input(digitalRead(SENSOR2));
  bool sensor1State = sensor1.output();
  bool sensor2State = sensor2.output();

  static bool prevs1;
  if (sensor1State != prevs1) {
    if (sensor1State) {
      logmsg("s1 is TRUE\n");
    } else {
      logmsg("s1 is false\n");
    }
    prevs1 = sensor1State;
  }
  static bool prevs2;
  if (sensor2State != prevs2) {
    if (sensor2State) {
      logmsg("s2 is TRUE\n");
    } else {
      logmsg("s2 is false\n");
    }
    prevs2 = sensor2State;
  }

  bool stateForLSM = sensor2State;
  digitalWrite(SENSORLED, stateForLSM);
  if (lsm.sensorState(stateForLSM)) {
    // If the state machine changes states... then transmit an update? hmm. No.
  }
}

void writePrefs()
{
  fs::File f = SPIFFS.open("/inator.cfg", "w");

  f.println("# Configuration for wash-is-done-inator");
  f.print("ssid=");
  f.println(Prefs.ssid);
  f.print("password=");
  f.println(Prefs.password);
  f.print("volume=");
  f.println(Prefs.volume);

  f.close();
}

bool readPrefs(fs::File f)
{
  bool readingVar = true;
  int8_t slen = 0;
  char lhs[50] = {'\0'};
  char *lp = lhs;
  char rhs[50] = {'\0'};
  char *rp = rhs;
  for(uint8_t i=0; i<f.size(); i++) {
    char c = f.read();
    // Simple safety for binary garbage
    if (c > 126 || (c<32 && c != 10 && c != 13))
      return false;
    
    // Skip commented out and blank lines
    if (slen == 0 && (c == '#' || c == '\n' || c == '\r')) {
      continue;
    }

    if (slen >= 49) {
      // safety: reset
      slen = 0;
      readingVar = true;
      lhs[0] = rhs[0] = '\0';
    }

    if (readingVar) {
      // Keep reading a variable name until we hit an '='
      if (c == '\n' || c == '\r') {
        // Abort - got a return before the '='
        slen = 0;
        lhs[0] = '\0';
      }
      else if (c == '=') {
        readingVar = false;
        slen = 0;
        rhs[0] = '\0';
      } else {
        lhs[slen++] = c;
        lhs[slen] = '\0';
      }
    } else {
      // Keep reading a variable value until we hit a newline
      if (c == '\n' || c == '\r') {
        processConfig(lhs,rhs);
        readingVar = true;
        slen = 0;
        lhs[0] = rhs[0] = '\0';
      } else {
        rhs[slen++] = c;
        rhs[slen] = '\0';
      }
    }
  }
  
  return true;
}

void processConfig(const char *lhs, const char *rhs)
{
  if (!strcmp(lhs, "ssid")) {
    strncpy(Prefs.ssid, (char *)rhs, sizeof(Prefs.ssid));
  }
  else if (!strcmp(lhs, "password")) {
    strncpy(Prefs.password, (char *)rhs, sizeof(Prefs.password));
  }
  else if (!strcmp(lhs, "volume")) {
    Prefs.volume = atof(rhs);
    lsm.setVolume(Prefs.volume);
  }
}

