#include <Arduino.h>
#include <WString.h>

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <WiFiServer.h>
#include <arduino_homekit_server.h>
#include <ESP8266mDNS.h>

#include "AudioFileSourceSPIFFS.h"
#include "lsm.h"
#include "debounce.h"

#include "musicplayer.h"
MusicPlayer musicPlayer;

bool fsRunning = false;
bool homekit_initialized = false;

LSM lsm;
Debounce sensor1, sensor2;

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define ALERTLED 1 // TX pin, GPIO 1. Blue.
#define SENSORLED 4 // D2 pin, GPIO 4. Green.

#define SENSOR1 16 // D0 pin, GPIO 16
#define SENSOR2 5 // D1 pin, GPIO 5

ESP8266WebServer server(80); //HTTP server on port 80
WiFiServer tcpserver(9001); // tcp server
WiFiClient tcpclient;

typedef struct _prefs {
  char ssid[50];
  char password[50];
  float volume;
  bool homeKitEnabled;

  // status, not really prefs... FIXME
  bool currentState;
  bool currentFault;
  bool currentActive;
  bool currentTampered;
  bool currentLowBattery;
} prefs;

prefs Prefs;
bool prefsOk = false;

static const char texthtml[] PROGMEM = "text/html";
static const char textplain[] PROGMEM = "text/plain";
static const char ftrue[] PROGMEM = "true";
static const char ffalse[] PROGMEM = "false";

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t sensorState;
//extern "C" homekit_characteristic_t statusFault;
//extern "C" homekit_characteristic_t statusActive;
//extern "C" homekit_characteristic_t statusTampered;
//extern "C" homekit_characteristic_t statusLowBattery;

void logmsg(const char *msg)
{
  if (tcpclient.connected()) {
    tcpclient.println(msg);
    tcpclient.flush();
  }
}

void SendHeader()
{
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, FPSTR(texthtml), "");
  fs::File f = SPIFFS.open("/header.html", "r");
  server.sendContent(f);
  f.close();
}

void SendFooter()
{
  fs::File f = SPIFFS.open("/footer.html", "r");
  server.sendContent(f);
  f.close();
}

void handleLs()
{
  SendHeader();
  
  File root = SPIFFS.open("/", "r");

  Dir dir = SPIFFS.openDir("/");
  server.sendContent(F("<pre>"));
  while (dir.next()) {
    server.sendContent(dir.fileName());
    server.sendContent(F("\n"));
  }
  server.sendContent(F("</pre>"));
  
  SendFooter();
}

void handleReset()
{
  SendHeader();

  server.sendContent(F("Okay, restarting"));

  SendFooter();
  
  ESP.restart();
}

void handleStatus()
{
  String status = String(F("<div>Uptime (millis): ")) +
    String(millis()) +
    String(F("</div><div>Free heap: ")) +
    String(ESP.getFreeHeap()) +
    String(F("</div><div>SSID: ")) +
    String(Prefs.ssid) +
    String(F("</div><div>Password: <span class='spoiler'>")) +
    String(Prefs.password) +
    String(F("</span></div><div>Volume: ")) +
    String(Prefs.volume) +
    String(F("</div><div>homeKit enabled: ")) +
    String(Prefs.homeKitEnabled ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>State: ")) +
    String(Prefs.currentState ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>Fault: ")) +
    String(Prefs.currentFault ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>Active: ")) +
    String(Prefs.currentActive ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>Tampered: ")) +
    String(Prefs.currentTampered ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>LowBattery: ")) +
    String(Prefs.currentLowBattery ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>LSM is alerting: ")) +
    String(lsm.isAlerting() ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>is playing: ")) +
    String(musicPlayer.isPlaying() ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>sensor1: ")) +
    String(sensor1.output() ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div><div>sensor2: ")) +
    String(sensor2.output() ? FPSTR(ftrue) : FPSTR(ffalse)) +
    String(F("</div>"));

  SendHeader();
  server.sendContent(status);
  SendFooter();
}

void handleTrigger()
{
  SendHeader();
  server.sendContent(F("Okay, triggered"));
  SendFooter();

  lsm.debugTrigger();
}

void handleConfig()
{
  SendHeader();
  
  String html =
    String(F("<form action='/submit' method='post'>"
             "<div><label for='ssid'>Connect to SSID:</label>"
             "<input type='text' id='ssid' name='ssid' value='")) +
    String(Prefs.ssid) +
    String(F("'/></div>"
             "<div><label for='password'>Network Password:</label>"
             "<input type='password' id='password' name='password' value='")) +
    String(Prefs.password) +
    String(F("'/></div>"
             "<div><label for='volume'>Volume (0.0-1.0):</label>"
             "<input type='number' id='volume' name='volume' step='0.01' value='")) +
    String(Prefs.volume) +
    String(F("'/></div>"
             "<div><label for='homeKitEnabled'>homeKitEnabled:</label>"
             "<input type='checkbox' name='homeKitEnabled' value='homeKitEnabled' ")) +
    String(Prefs.homeKitEnabled ? "checked" : "") +
    String(F("'/></div>"
             // DEBUG: dump of settings for HomeKit testing
             "<div><label for='currentState'>currentState:</label>"
             "<input type='checkbox' name='currentState' value='currentState'/></div>"
             "<div><label for='currentFault'>currentFault:</label>"
             "<input type='checkbox' name='currentFault' value='currentFault'/></div>"
             "<div><label for='currentActive'>currentActive:</label>"
             "<input type='checkbox' name='currentActive' value='currentActive'/></div>"
             "<div><label for='currentTampered'>currentTampered:</label>"
             "<input type='checkbox' name='currentTampered' value='currentTampered'/></div>"
             "<div><label for='currentLowBattery'>currentLowBattery:</label>"
             "<input type='checkbox' name='currentLowBattery' value='currentLowBattery'/></div>"
             // END DEBUG
             "<div><input type='submit' value='Save' /></div>"));

  server.sendContent(html);
  
  SendFooter();
}

void handleSubmit()
{
  String new_ssid = server.arg("ssid");
  String new_password = server.arg("password");
  String new_volume = server.arg("volume");
  strncpy(Prefs.ssid, new_ssid.c_str(), sizeof(Prefs.ssid));
  strncpy(Prefs.password, new_password.c_str(), sizeof(Prefs.password));
  Prefs.volume = atof(new_volume.c_str());
  lsm.setVolume(Prefs.volume);

  Prefs.homeKitEnabled = server.arg("homeKitEnabled").isEmpty() ? 0 : 1;
  // Start homekit if needed. Stopping it is messier - we just stop sending data,
  // but on reboot we'd not even register.
  if (Prefs.homeKitEnabled && !homekit_initialized) {
    arduino_homekit_setup(&config);
    homekit_initialized = true;
  }
  
  Prefs.currentState = server.arg("currentState").isEmpty() ? 0 : 1;
  Prefs.currentFault = server.arg("currentFault").isEmpty() ? 0 : 1;
  Prefs.currentActive = server.arg("currentActive").isEmpty() ? 0 : 1;
  Prefs.currentTampered = server.arg("currentTampered").isEmpty() ? 0 : 1;
  Prefs.currentLowBattery = server.arg("currentLowBattery").isEmpty() ? 0 : 1;
  
  writePrefs();

  // Redirect to /status to show the changes
  server.sendHeader(F("Location"), String("/status"), true);
  server.send(302, FPSTR(textplain), "");
}

void handleStop()
{
  SendHeader();
  server.sendContent(F("Stopping"));
  SendFooter();

  lsm.buttonPressed();
  musicPlayer.stop();
}

void handleDebug()
{
  SendHeader();
  server.sendContent(F("Debug hook called - starting music playing"));
  SendFooter();

  musicPlayer.start(Prefs.volume);
}

void handleDownload()
{

  String filename = server.arg("file");
  if (filename.isEmpty()) {
    server.send(200,
                FPSTR(texthtml), 
                F("<html><h3>Error</h3><div>No file argument specified</div></html>"));
    return;
  }
  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }
  
  fs::File f = SPIFFS.open(filename, "r");
  if (!f) {
    server.send(200,
                FPSTR(texthtml),
                F("<html><h3>Error</h3><div>File not found</div></html>"));
    return;
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, F("application/octet-stream"), "");
  uint32_t fsize = f.size();
  char buf[128];
  while (fsize) {
    uint32_t count = f.readBytes(buf, MIN(sizeof(buf), fsize));
    fsize -= count;
    server.sendContent(buf, count);
  }
  f.close();
}

fs::File fsUploadFile;
void handleUpload()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    fsUploadFile = SPIFFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
  }
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

  // Turn TX/RX in to GPIO1 and GPIO 3
  pinMode(1, FUNCTION_3);
  pinMode(3, FUNCTION_3);

  pinMode(ALERTLED, OUTPUT);
  pinMode(SENSORLED, OUTPUT);

  pinMode(SENSOR1, INPUT);
  pinMode(SENSOR2, INPUT);

  pinMode(A0, INPUT);

  // Use alert/sensor LEDs to show startup sequence progress
  digitalWrite(ALERTLED, LOW);
  digitalWrite(SENSORLED, LOW);
  
  // Set some default preferences in case we can't load any prefs...
  strncpy(Prefs.ssid, "", sizeof(Prefs.ssid));
  strncpy(Prefs.password, "", sizeof(Prefs.password));
  Prefs.volume = 0.05; // Default to a low but audible volume
  lsm.setVolume(Prefs.volume);

  Prefs.currentState = Prefs.currentFault = Prefs.currentActive = Prefs.currentTampered = Prefs.currentLowBattery = false;
  
  if (fsRunning) {
    // Try to load the config file
    fs::File f = SPIFFS.open("/inator.cfg", "r");
    if (!f) {
      // No config file found; create a set of defaults and save it
      writePrefs();
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


  digitalWrite(ALERTLED, HIGH);
  digitalWrite(SENSORLED, LOW);
  
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
      // Failed to connect to wifi. Blink LEDs and then start up a new wifi.
      while (count--) {
        digitalWrite(ALERTLED, HIGH);
        digitalWrite(SENSORLED, LOW);
        delay(100);
        digitalWrite(ALERTLED, LOW);
        digitalWrite(SENSORLED, HIGH);
        delay(100);
      }
      StartSoftAP();
    }
  }
  
  digitalWrite(ALERTLED, HIGH);
  digitalWrite(SENSORLED, HIGH);
  
  ArduinoOTA.setHostname("doneinator");
  ArduinoOTA.begin();

  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.on("/config", handleConfig);
  server.on("/submit", handleSubmit);
  server.on("/reset", handleReset);
  server.on("/status", handleStatus);
  server.on("/trigger", handleTrigger);
  server.on("/stop", handleStop);
  server.on("/debug", handleDebug);
  server.on("/ls", handleLs);
  server.on("/download", handleDownload);
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "{\"success\":1}");
  }, handleUpload);
  
  server.begin();
  tcpserver.begin();
  
  MDNS.begin("doneinator");
  MDNS.addService("http", "tcp", 80);

  if (Prefs.homeKitEnabled) {
    arduino_homekit_setup(&config);
    homekit_initialized = true;
  }
  
  digitalWrite(ALERTLED, LOW);
  digitalWrite(SENSORLED, LOW);
}

bool buttonIsPressed()
{
  return (analogRead(A0) < (1023/2));
}

void my_homekit_loop()
{
  if (Prefs.homeKitEnabled) {
    arduino_homekit_loop();
    static uint32_t nextReport = 0;
    if (millis() > nextReport) {
      sensorState.value.int_value = Prefs.currentState ? 1 : 0;
      //    statusFault.value.int_value = Prefs.currentFault ? 1 : 0;
      //    statusActive.value.int_value = Prefs.currentActive ? 1 : 0;
      //    statusTampered.value.int_value = Prefs.currentTampered ? 1 : 0;
      //    statusLowBattery.value.int_value = Prefs.currentLowBattery ? 1 : 0;
      homekit_characteristic_notify(&sensorState, sensorState.value);
      //    homekit_characteristic_notify(&statusFault, statusFault.value);
      //    homekit_characteristic_notify(&statusActive, statusActive.value);
      //    homekit_characteristic_notify(&statusTampered, statusTampered.value);
      //    homekit_characteristic_notify(&statusLowBattery, statusLowBattery.value);
      nextReport = millis() + 10000; // 10 seconds
    }
  }
}


void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  
  if (homekit_initialized && Prefs.homeKitEnabled) {
    my_homekit_loop();
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
    Prefs.currentState = true; // in the next poll loop, we'll update HomeKit
  } else {
    digitalWrite(ALERTLED, LOW);
    Prefs.currentState = false;
  }
  
  musicPlayer.maint();

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
  f.print("homeKitEnabled=");
  f.println(Prefs.homeKitEnabled ? "1" : "0");

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
  else if (!strcmp(lhs, "homeKitEnabled")) {
    Prefs.homeKitEnabled = atoi(rhs) ? true : false;
  }
}

