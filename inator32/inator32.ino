#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <HomeSpan.h>

#include <WebServer.h>

#include "debounce.h"

#include "templater.h"
Templater templ;

bool fsRunning;
bool softAPMode = false;
bool homekit_initialized = false;

Debounce sensor1, sensor2;

static const char texthtml[] PROGMEM = "text/html";
static const char textplain[] PROGMEM = "text/plain";
static const char ftrue[] PROGMEM = "true";
static const char ffalse[] PROGMEM = "false";

WebServer server(80);

#ifndef MIN
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#endif

typedef struct _prefs {
  char ssid[50];
  char password[50];
  float volume;
  bool homeKitEnabled;
  bool audioNotificationEnabled;

  // status, not really prefs... FIXME
  bool currentState;
  uint32_t lastS1Change;
  uint32_t lastS2Change;

} prefs;

prefs Prefs;
bool prefsOk = false;

void SendHeader()
{
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, FPSTR(texthtml), "");
  fs::File f = SPIFFS.open("/header.html", "r");
  server.sendContent(f.readString());
  f.close();
}

void SendFooter()
{
  fs::File f = SPIFFS.open("/footer.html", "r");
  server.sendContent(f.readString());
  f.close();
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
             "<div><label for='audioEnabled'>audioAlertEnabled:</label>"
             "<input type='checkbox' name='audioEnabled' value='audioEnabled' ")) +
    String(Prefs.audioNotificationEnabled ? "checked" : "") +
    String(F("/></div>"
             // DEBUG: dump of settings for HomeKit testing
             "<div><label for='currentState'>currentState:</label>"
             "<input type='checkbox' name='currentState' value='currentState'/></div>"
             // END DEBUG
             "<div><input type='submit' value='Save' /></div>"
             "</form>"
             ));

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

  Prefs.homeKitEnabled = server.arg("homeKitEnabled").isEmpty() ? 0 : 1;
  // Start homekit if needed. Stopping it is messier - we just stop sending data,
  // but on reboot we'd not even register.
  if (Prefs.homeKitEnabled && !homekit_initialized) {
    initHomeSpan();
    homekit_initialized = true;
  }

  Prefs.audioNotificationEnabled = server.arg("audioEnabled").isEmpty() ? 0 : 1;
  Prefs.currentState = server.arg("currentState").isEmpty() ? 0 : 1;

  writePrefs();

  // Redirect to /status to show the changes
  server.sendHeader(F("Location"), String("/status"), true);
  server.send(302, FPSTR(textplain), "");
}

void handleStatus()
{
  SendHeader();

  repvars *r = templ.newRepvar(String("@UPTIME@"), String(millis()));
  r = templ.addRepvar(r, String("@HEAP@"), String(ESP.getFreeHeap()));
  r = templ.addRepvar(r, String("@SSID@"), String(Prefs.ssid));
  r = templ.addRepvar(r, String("@PASS@"), String(Prefs.password));
  r = templ.addRepvar(r, String("@VOL@"), String(Prefs.volume));
  r = templ.addRepvar(r, String("@HK@"), String(Prefs.homeKitEnabled ? FPSTR(ftrue) : FPSTR(ffalse)));
  r = templ.addRepvar(r, String("@AN@"), String(Prefs.audioNotificationEnabled ? FPSTR(ftrue) : FPSTR(ffalse)));
  r = templ.addRepvar(r, String("@STATE@"), String(Prefs.currentState ? FPSTR(ftrue) : FPSTR(ffalse)));
  //  r = templ.addRepvar(r, String("@ALERT@"), String(lsm.isAlerting()));
  //  r = templ.addRepvar(r, String("@PLAY@"), String(musicPlayer.isPlaying()));
  uint8_t s1o = sensor1.output();
  uint8_t s2o = sensor2.output();
  /*
  r = templ.addRepvar(r, String("@S1@"), String((s1o == isOff) ? "Off" :
                                                ( (s1o == isBlinking) ? "Blinking" :
                                                  ( (s1o == isOn) ? "On" : "Unknown"))));
  r = templ.addRepvar(r, String("@S2@"), String((s2o == isOff) ? "Off" :
                                                ( (s2o == isBlinking) ? "Blinking" :
                                                  ( (s2o == isOn) ? "On" : "Unknown"))));
  */
  r = templ.addRepvar(r, String("@S1T@"), String((millis() - Prefs.lastS1Change)/1000));
  r = templ.addRepvar(r, String("@S2T@"), String((millis() - Prefs.lastS2Change)/1000));
  //  r = templ.addRepvar(r, String("@DryState@"), String(lsm.lastDryerState()));
  //  r = templ.addRepvar(r, String("@WashState@"), String(lsm.lastWasherState()));

  fs::File f = SPIFFS.open("/status.html", "r");
  templ.generateOutput(&server, f, r);
  f.close();

  SendFooter();
}

void handleLs()
{
  SendHeader();

  File root = SPIFFS.open("/", "r");

  File file = root.openNextFile();
  server.sendContent(F("<pre>"));
  while (file) {
    server.sendContent(file.name());
    server.sendContent(F("\n"));
    file = root.openNextFile();
  }
  server.sendContent(F("</pre>"));

  SendFooter();
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
  }
  else if (!strcmp(lhs, "homeKitEnabled")) {
    Prefs.homeKitEnabled = atoi(rhs) ? true : false;
  }
  else if (!strcmp(lhs, "audioNotificationEnabled")) {
    Prefs.audioNotificationEnabled = atoi(rhs) ? true : false;
  }
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
  f.print("audioNotificationEnabled=");
  f.println(Prefs.audioNotificationEnabled ? "1" : "0");
  f.close();
}

void StartSoftAP()
{
  WiFi.mode(WIFI_AP);
  //  WiFi.setPhyMode((WiFiPhyMode_t)PHY_MODE_11N);
  WiFi.softAP("WashIsDoneInator2");
  // Set the IP address and info for SoftAP mode. Note this is also
  // the default IP (192.168.4.1), but better to be explicit...
  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  softAPMode = true;
}

struct WashDrySensor : Service::ContactSensor {
  SpanCharacteristic *isOn;
  WashDrySensor() : Service::ContactSensor() {
    isOn = new Characteristic::ContactSensorState(0, false); // default of 0, and not persistent
  }
};

struct WashDrySensor *wds = NULL;

void initHomeSpan()
{
  // Don't try to initialize HomeSpan if we aren't on the target network
  if (softAPMode)
    return;
  
  homeSpan.setWifiCredentials(Prefs.ssid, Prefs.password);
  homeSpan.setQRID("M3E2");
  homeSpan.setPortNum(8080);
  
  homeSpan.begin(Category::Sensors, "HomeSpan-Washinator", "Washinator");
  
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  wds = new WashDrySensor();
}

void setup()
{
  delay(1000); // Let power settle
  Serial.begin(115200);
  delay(1000);
  Serial.println("Hello world");
  
  if (SPIFFS.begin()) {
    fsRunning = true;
  } else {
    fsRunning = false;
  }

  // Set some default preferences in case we can't load any prefs...
  Serial.println("Setting up default prefs");
  delay(100);
  strncpy(Prefs.ssid, "", sizeof(Prefs.ssid));
  strncpy(Prefs.password, "", sizeof(Prefs.password));
  Prefs.volume = 0.05; // Default to a low but audible volume
  
  Prefs.currentState = false;
  Prefs.lastS1Change = Prefs.lastS2Change = 0;

  Serial.println("reading old config");
  delay(100);
  if (fsRunning) {
    fs::File f = SPIFFS.open("/inator.cfg", "r");
    if (!f) {
      // no config file found; write defaults
      writePrefs();
      f = SPIFFS.open("/inator.cfg", "r");
      if (f) {
        prefsOk = true;
      }
    }
    if (f) {
      prefsOk = readPrefs(f);
    }
  }

  Serial.println("Starting wifi");
  delay(100);
  if (!prefsOk ||
      prefsOk && !strlen(Prefs.ssid)) {
    Serial.println("Prefs invalid; starting SoftAP");
  delay(100);
    StartSoftAP();
  } else {
    Serial.print("Connecting to ");
    Serial.println(Prefs.ssid);
  delay(100);
    WiFi.mode(WIFI_STA);
    //    WiFi.setPhyMode((WiFiPhyMote_T)PHY_MODE_11N);
    WiFi.begin(Prefs.ssid, Prefs.password);
    uint8_t count=0;
    while (count < 10 && WiFi.waitForConnectResult() != WL_CONNECTED) {
      count++;
      delay(5000);
    }
    if (count >= 10) {
      // Failed to connect to wifi; start softAP instead.
      Serial.println("Failed to connect; reverting to SoftAP");
  delay(100);
      StartSoftAP();
    }
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  //  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.println("Setting up OTA updates");
  delay(100);
  ArduinoOTA.setHostname("doneinator-2");
  ArduinoOTA.begin();

  Serial.println("Wiring together web server");
  delay(100);
  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/style.css", SPIFFS, "/style.css");
  server.on("/config", handleConfig);
  server.on("/submit", handleSubmit);
  server.on("/status", handleStatus);
  server.on("/ls", handleLs);
  server.on("/download", handleDownload);
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "{\"success\":1}");
  }, handleUpload);

  server.begin();

  Serial.println("Registering with multicast DNS");
  delay(100);
  MDNS.begin("doneinator-2");
  MDNS.addService("http", "tcp", 80);
  
  if (Prefs.homeKitEnabled) {
    Serial.println("Initializing homekit");
    delay(100);
    initHomeSpan();
    homekit_initialized = true;
  }

  Serial.println("Off to the races");
  delay(1000);
}

void wifi_stayConnected()
{
  if (softAPMode)
    return;

  static uint32_t nextMillis = 0;
  if (millis() > nextMillis) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(Prefs.ssid, Prefs.password);
    }

    //    MDNS.announce();

    nextMillis = millis() + 15000;
  }
}

void my_homekit_loop()
{
  homeSpan.poll();

  // Set the current device state based on the alerting or whatnot
  static bool previousState = Prefs.currentState;
  if (previousState != Prefs.currentState) {
    previousState = Prefs.currentState;
    if (wds) {
      Serial.print("Notifying HomeKit of changed state ");
      Serial.println(Prefs.currentState ? 1 : 0);
      wds->isOn->setVal(Prefs.currentState ? 1 : 0);
    }
  }
}

void loop()
{
  wifi_stayConnected();
  ArduinoOTA.handle();
  server.handleClient();
  
  if (homekit_initialized && Prefs.homeKitEnabled) {
    my_homekit_loop();
  }
}
