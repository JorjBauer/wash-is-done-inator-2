#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

#include <WebServer.h>

#include "debounce.h"
#include "lsm.h"
#include "delayedsensor.h"

#include "templater.h"
Templater templ;

const int pin_sensor1 = A1;
const int pin_sensor2 = A2;
const int pin_switch = A3;
const int pin_led1 = MISO; // aka A12, pin 13
const int pin_led2 = MOSI; // aka A10, pin 11

#define ALERT_LED pin_led2
#define SENSOR_LED pin_led1

bool fsRunning;
bool softAPMode = false;

LSM washerLSM, drierLSM;
Debounce rawsensor1, rawsensor2;
DelayedSensor sensor1, sensor2;
uint8_t s1out, s2out;
Debounce rawButton;

static const char texthtml[] PROGMEM = "text/html";
static const char textplain[] PROGMEM = "text/plain";
static const char ftrue[] PROGMEM = "true";
static const char ffalse[] PROGMEM = "false";

WebServer server(80);
WiFiServer tcpserver(9001); // tcp server
WiFiClient tcpclient;

#ifndef MIN
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#endif

char bigbuf[150];

typedef struct _prefs {
  char ssid[50];
  char password[50];
  float volume;
  bool discordEnabled;
  char discordURL[150];
  bool audioNotificationEnabled;

  // status, not really prefs... FIXME
  bool currentState;
  uint32_t lastS1Change;
  uint32_t lastS2Change;

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
             "<div><label for='discordEnabled'>discordEnabled:</label>"
             "<input type='checkbox' name='discordEnabled' value='discordEnabled' ")) +
    String(Prefs.discordEnabled ? "checked" : "") +
    String(F("'/></div>"
             "<div><label for='discordURL'>Discord Webhook URL:</label>"
             "<input type='text' id='discordURL' name='discordURL' value='")) +
    String(Prefs.discordURL) +
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
  String new_discordURL = server.arg("discordURL");
  strncpy(Prefs.ssid, new_ssid.c_str(), sizeof(Prefs.ssid));
  strncpy(Prefs.password, new_password.c_str(), sizeof(Prefs.password));
  Prefs.volume = atof(new_volume.c_str());

  Prefs.discordEnabled = server.arg("discordEnabled").isEmpty() ? 0 : 1;
  strncpy(Prefs.discordURL, new_discordURL.c_str(), sizeof(Prefs.discordURL));

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
  r = templ.addRepvar(r, String("@DSE@"), String(Prefs.discordEnabled ? FPSTR(ftrue) : FPSTR(ffalse)));
  r = templ.addRepvar(r, String("@DISCORDURL@"), String(Prefs.discordURL));
  r = templ.addRepvar(r, String("@AN@"), String(Prefs.audioNotificationEnabled ? FPSTR(ftrue) : FPSTR(ffalse)));
  r = templ.addRepvar(r, String("@STATE@"), String(Prefs.currentState ? FPSTR(ftrue) : FPSTR(ffalse)));
  r = templ.addRepvar(r, String("@ALERT@"), String(washerLSM.isAlerting()));
  //  r = templ.addRepvar(r, String("@PLAY@"), String(musicPlayer.isPlaying()));
  r = templ.addRepvar(r, String("@S1@"), String((s1out == isOff) ? "Off" :
                                                ( (s1out == isBlinking) ? "Blinking" :
                                                  ( (s1out == isOn) ? "On" : "Unknown"))));
  r = templ.addRepvar(r, String("@S2@"), String((s2out == isOff) ? "Off" :
                                                ( (s2out == isBlinking) ? "Blinking" :
                                                  ( (s2out == isOn) ? "On" : "Unknown"))));

  r = templ.addRepvar(r, String("@S1T@"), String((millis() - Prefs.lastS1Change)/1000));
  r = templ.addRepvar(r, String("@S2T@"), String((millis() - Prefs.lastS2Change)/1000));
  r = templ.addRepvar(r, String("@DryState@"), String(drierLSM.lastState()));
  r = templ.addRepvar(r, String("@WashState@"), String(washerLSM.lastState()));

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
  else if (!strcmp(lhs, "audioNotificationEnabled")) {
    Prefs.audioNotificationEnabled = atoi(rhs) ? true : false;
  }
  else if (!strcmp(lhs, "discordEnabled")) {
    Prefs.discordEnabled = atoi(rhs) ? true : false;
  }
  else if (!strcmp(lhs, "discordURL")) {
    strncpy(Prefs.discordURL, (char *)rhs, sizeof(Prefs.discordURL));
  }
}

bool readPrefs(fs::File f)
{
  bool readingVar = true;
  int16_t slen = 0;
  char lhs[50] = {'\0'};
  char *lp = lhs;
  bigbuf[0] = 0;
  char *rp = bigbuf;
  Serial.print("Prefs file size: ");
  Serial.println(f.size());
  for(uint16_t i=0; i<f.size(); i++) {
    char c = f.read();
    // Simple safety for binary garbage                                                                                                                 
    if (c > 126 || (c<32 && c != 10 && c != 13))
      {
        Serial.print("read weird char ");
        Serial.println((int)c);
        return false;
      }
    
    // Skip commented out and blank lines
    if (slen == 0 && readingVar && (c == '#' || c == '\n' || c == '\r')) {
      continue;
    }

    if (slen >= sizeof(bigbuf)-1) {
      // safety: reset
      slen = 0;
      readingVar = true;
      lhs[0] = bigbuf[0] = '\0';
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
        bigbuf[0] = '\0';
      } else {
        lhs[slen++] = c;
        lhs[slen] = '\0';
      }
    } else {
      // Keep reading a variable value until we hit a newline                                                                                           
      if (c == '\n' || c == '\r') {
        processConfig(lhs,bigbuf);
        readingVar = true;
        slen = 0;
        lhs[0] = bigbuf[0] = '\0';
      } else {
        bigbuf[slen++] = c;
        bigbuf[slen] = '\0';
      }
    }
  }

  processConfig("ssid", "edelman");
  processConfig("password", "casadepepe");
  processConfig("discordEnabled", "1");
  //  processConfig("discordURL", "https://discord.com/api/webhooks/1043953512565981284/iIU7ewRfQfY8_-tBr7Nfel9XQl5WUXhqhDCCbNRg5cPBeGUb0VtLcXIybrsG1-6yssza");
  
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
  f.print("discordEnabled=");
  f.println(Prefs.discordEnabled ? "1" : "0");
  f.print("discordURL=");
  f.println(Prefs.discordURL);
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

void setup()
{
  Serial.begin(115200);
  delay(1000); // Let power settle
  Serial.println("Hello world");
  delay(3000); // let serial catch up
  
  if (SPIFFS.begin()) {
    fsRunning = true;
    Serial.println("SPIFFS is started");
  } else {
    fsRunning = false;
    Serial.println("failed to start SPIFFS");
  }

  pinMode(pin_sensor1, INPUT);
  pinMode(pin_sensor2, INPUT);
  // analogSetAttenuation(ADC_11db); // 11db is the max and is the default (~2.6v)  
  pinMode(pin_led1, OUTPUT);
  pinMode(pin_led2, OUTPUT);
  pinMode(pin_switch, INPUT_PULLUP);
  
  // Set some default preferences in case we can't load any prefs...
  Serial.println("Setting up default prefs");
  delay(100);
  strncpy(Prefs.ssid, "", sizeof(Prefs.ssid));
  strncpy(Prefs.password, "", sizeof(Prefs.password));
  Prefs.volume = 0.05; // Default to a low but audible volume
  
  Prefs.currentState = false;
  Prefs.lastS1Change = Prefs.lastS2Change = 0;

  delay(100);
  if (fsRunning) {
    fs::File f = SPIFFS.open("/inator.cfg", "r");
    if (!f) {
      // no config file found; write defaults
      Serial.println("Unable find prefs file; writing/setting defaults");
      writePrefs();
      f = SPIFFS.open("/inator.cfg", "r");
      if (f) {
        prefsOk = readPrefs(f); // go through the motions of re-reading to ensure it's possible
      }
    }
    if (f) {
      Serial.println("reading old config");
      prefsOk = readPrefs(f);
      if (!prefsOk) {
        Serial.println("failed to read config. Writing defaults...");
        writePrefs();
        if (!readPrefs(f)) {
          Serial.println("Something is very wrong; can't re-read prefs after having written.");
        }
      }
    }
  }
  Serial.print("Prefs status: ");
  Serial.println(prefsOk ? "ok" : "bad");

  prefsOk = true; // debug FIXME
  
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
  WiFi.setSleep(false);
  //WiFi.setSleepMode(WIFI_NONE_SLEEP);

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
  server.on("/restart", handleRestart);

  server.begin();
  tcpserver.begin();

  Serial.println("Registering with multicast DNS");
  delay(100);
  MDNS.begin("doneinator-2");
  MDNS.addService("http", "tcp", 80);
  
  Serial.println("Initializing sensors");
  washerLSM.reset();
  drierLSM.reset();
  
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

int sendDiscordAlert(String msg)
{
  if (!Prefs.discordURL[0]) {
    Serial.println("Unable to send Discord alert; no webhook URL configured");
    return 503;
  }

  HTTPClient http;
  http.begin(Prefs.discordURL);
  http.addHeader("Content-Type", "application/json");

  JSONVar requestObject;
  requestObject["content"] = msg.c_str();

  int httpResponseCode = http.POST(JSON.stringify(requestObject));
  http.end();
  
  return httpResponseCode;
}

void my_discord_loop()
{
  static bool lastAlertingState = false;
  static uint32_t nextNotice = 0;

  if (washerLSM.isAlerting() != lastAlertingState) {
    Serial.print("change status from ");
    Serial.print(lastAlertingState ? "true" : "false");
    Serial.print(" to ");
    Serial.println(washerLSM.isAlerting() ? "true" : "false");
    lastAlertingState = washerLSM.isAlerting();
    if (lastAlertingState) {
      nextNotice = 0; // reset notice so we'll send something now
    } else {
      // We only alert once on close/end-of-alert
      sendDiscordAlert("Washinator is (closed)");
    }
  }
  if (lastAlertingState && (millis() >= nextNotice)) {
    nextNotice = millis() + 10 * 60 * 1000; // 10 minutes
    sendDiscordAlert("Washinator is (open)");
  }
}

void sensor_loop()
{
  uint16_t raw1 = analogRead(pin_sensor1);
  bool tmpState1 = raw1 >= 3000;
  rawsensor1.input(tmpState1);
  uint16_t raw2 = analogRead(pin_sensor2);
  bool tmpState2 = raw2 >= 3500;
  rawsensor2.input(tmpState2);

  bool raw1out = rawsensor1.output();
  bool raw2out = rawsensor2.output();
  
  s1out = sensor1.sensorState(raw1out);
  s2out = sensor2.sensorState(raw2out);

  //  static char buf[50];
  
  static bool prevs1;
  if (tmpState1 != prevs1) {
    Prefs.lastS1Change = millis();

    /*
    if (raw1out) {
      sprintf(buf, "s1 is TRUE: %d\n", raw1);
    } else {
      sprintf(buf, "s1 is false: %d\n", raw1);
      }
      logmsg(buf);*/
    prevs1 = tmpState1;
  }
  static bool prevs2;
  if (tmpState2 != prevs2) {
    Prefs.lastS2Change = millis();
    /*
    if (tmpState2) {
      sprintf(buf, "s2 is TRUE: %d\n", raw2);
    } else {
      sprintf(buf, "s2 is false: %d\n", raw2);
    }
    logmsg(buf);*/
    prevs2 = tmpState2;
  }

  // The green LED is on whenever either of the sensors are on
  // FIXME: just tracking sensor 1 right now
  digitalWrite(SENSOR_LED, raw1out | raw2out); // FIXME which? both?

  // The LSM takes both sensor states (washer first, then dryer)
  // FIXME: only sensor1 right now
  if (washerLSM.sensorState(s1out)) {
    // If the state machine changes states... then transmit an update? hmm. No.
  }
  
}

void button_loop()
{
  // If we're delaying before re-alterting or playing the tune, and
  // the button's pressed, then abort; that's confirmation that alert
  // was received
  if (washerLSM.isAlerting() && buttonIsPressed()) {
    washerLSM.buttonPressed();
    // Shut off the LEDs and delay a short time so we don't accidentally
    // trigger the sensor on our own LEDs
    //    digitalWrite(ALERT_LED, LOW);
    //    digitalWrite(SENSOR_LED, LOW);
    delay(200);
    return; // abandon this run - do not update lsm's sensor readings
  }
}


void loop()
{
  wifi_stayConnected();
  ArduinoOTA.handle();
  server.handleClient();

  // Track the alerting state with the alert LED
  if (washerLSM.isAlerting()) {
    digitalWrite(ALERT_LED, HIGH);
    Prefs.currentState = true;
  } else {
    digitalWrite(ALERT_LED, LOW);
    Prefs.currentState = false;
  }

  if (Prefs.discordEnabled) {
    my_discord_loop();
  }

  if (tcpserver.hasClient()) {
    if (tcpclient.connected()) {
      tcpclient.stop(); // only one debugging session at a time please
    }
    tcpclient = tcpserver.available();
    tcpclient.println("Hello");
    tcpclient.flush();
  }

  sensor_loop();
  button_loop();
}

bool buttonIsPressed()
{
  // FIXME this pullup looks terrible; I'm reading 300 -> 1800 sort of values (out of 4095 for 3.3v)
  uint16_t r = analogRead(pin_switch);
  rawButton.input(r < 100);
  return rawButton.output();
  //  return (analogRead(pin_switch) < (2*4095/3));
}

// The LSM object calls beginAlert(this) or endAlert(this)
void beginAlert(void *o)
{
}

void endAlert(void *o)
{
}

void handleRestart()
{
  ESP.restart();
}

