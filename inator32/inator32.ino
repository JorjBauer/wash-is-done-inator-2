#include <Arduino.h>
#include <time.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <esp_wifi.h>

#include "WebManager.h"
#include "WifiManager.h"
#include "TCPLogger.h"
#include "WashPrefs.h"

#include "debounce.h"
#include "delayedsensor.h"

#include "templater.h"
extern Templater templ;

#define NAME "inator"

const int pin_sensor1 = A1;
const int pin_sensor2 = A2;
const int pin_switch = A3;
const int pin_led1 = MISO; // aka A12, pin 13
const int pin_led2 = MOSI; // aka A10, pin 11

#define ALERT_LED pin_led2
#define SENSOR_LED pin_led1

bool fsRunning;

Debounce rawsensor1, rawsensor2;
DelayedSensor washerSensor("washer"), dryerSensor("dryer");
uint8_t s1out, s2out;
Debounce rawButton;

#ifndef MIN
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#endif

bool currentState;
uint32_t lastS1Change;
uint32_t lastS2Change;

WashPrefs myprefs;
TCPLogger tlog;
WebManager server(80);
WifiManager wifi;

void handleLocalConfig()
{
  server.SendHeader();

  repvars *r = templ.newRepvar(String("@DU@"), String(myprefs.discordURL));
  r = templ.addRepvar(r, String("@DE@"), String(myprefs.discordEnabled ? "checked" : ""));
  r = templ.addRepvar(r, String("@CS@"), String(currentState ? "checked" : ""));
  r = templ.addRepvar(r, String("@WURL@"), String(myprefs.washerNotificationURL));
  r = templ.addRepvar(r, String("@DURL@"), String(myprefs.dryerNotificationURL));
    
  fs::File f = SPIFFS.open("/config2.html", "r");
  templ.generateOutput(&server, f, r);
  f.close();

  server.SendFooter();
}

void handleLocalSubmit()
{
  String new_discordURL = server.arg("discordURL");
  String new_washerURL = server.arg("washerNotificationURL");
  String new_dryerURL = server.arg("dryerNotificationURL");

  myprefs.discordEnabled = server.arg("discordEnabled").isEmpty() ? 0 : 1;
  strncpy(myprefs.discordURL, new_discordURL.c_str(), sizeof(myprefs.discordURL));
  strncpy(myprefs.washerNotificationURL, new_washerURL.c_str(), sizeof(myprefs.washerNotificationURL));
  strncpy(myprefs.dryerNotificationURL, new_dryerURL.c_str(), sizeof(myprefs.dryerNotificationURL));

  currentState = server.arg("currentState").isEmpty() ? 0 : 1;

  myprefs.write();

  // Redirect to /status to show the changes
  server.sendHeader(F("Location"), String("/status2"), true);
  server.send(302, "text/plain", "");
}

void handleLocalStatus()
{
  server.SendHeader();

  repvars *r = templ.newRepvar(String("@DISCORDURL@"), String(myprefs.discordURL));
  r = templ.addRepvar(r, String("@WASHERURL@"), String(myprefs.washerNotificationURL));
  r = templ.addRepvar(r, String("@DRYERURL@"), String(myprefs.dryerNotificationURL));
  r = templ.addRepvar(r, String("@DSE@"), String(myprefs.discordEnabled ? "true" : "false"));
  r = templ.addRepvar(r, String("@STATE@"), String(currentState ? "true" : "false"));
  r = templ.addRepvar(r, String("@S1@"), String((s1out == isOff) ? "Off" :
                                                ( (s1out == isBlinking) ? "Blinking" :
                                                  ( (s1out == isOn) ? "On" : "Unknown"))));
  r = templ.addRepvar(r, String("@S2@"), String((s2out == isOff) ? "Off" :
                                                ( (s2out == isBlinking) ? "Blinking" :
                                                  ( (s2out == isOn) ? "On" : "Unknown"))));

  r = templ.addRepvar(r, String("@S1T@"), String((millis() - lastS1Change)/1000));
  r = templ.addRepvar(r, String("@S2T@"), String((millis() - lastS2Change)/1000));
  r = templ.addRepvar(r, String("@DryState@"), dryerSensor.getCurrentAnalysisAsString());
  r = templ.addRepvar(r, String("@WashState@"), washerSensor.getCurrentAnalysisAsString());

  fs::File f = SPIFFS.open("/status2.html", "r");
  templ.generateOutput(&server, f, r);
  f.close();

  server.SendFooter();
}

void handleStop()
{
  // Redirect to /status to show the changes
  server.sendHeader(F("Location"), String("/status"), true);
  server.send(302, "text/plain", "");
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

  currentState = false;
  lastS1Change = lastS2Change = 0;

  myprefs.begin(NAME);
  myprefs.setDefaults();

  if (fsRunning) {
    myprefs.read();
  }

  Serial.println("Starting wifi");
  delay(100);
  wifi.begin(&myprefs, NAME);
  // esp_wifi_set_max_tx_power(50); // FIXME hack to test a theory
  server.begin(&myprefs);
  tlog.begin();

  MDNS.begin(myprefs.mdnsName);
  MDNS.addService("http", "tcp", 80);

  server.on("/stop", handleStop);
  server.on("/status2", handleLocalStatus);
  server.on("/config2", handleLocalConfig);
  server.on("/submit2", handleLocalSubmit);  
  
  Serial.println("Initializing sensors");
  
  Serial.println("Off to the races");
  delay(1000);
}

int sendDiscordAlert(String msg)
{
  if (!myprefs.discordURL[0]) {
    Serial.println("Unable to send Discord alert; no webhook URL configured");
    return 503;
  }

  HTTPClient http;
  http.begin(myprefs.discordURL);
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

  /*
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
  */
  if (lastAlertingState && (millis() >= nextNotice)) {
    nextNotice = millis() + 10 * 60 * 1000; // 10 minutes
    sendDiscordAlert("Washinator is (open)");
  }
}

void sensor_loop()
{
  // FIXME: for both of these sensors, the analogRead values are
  // somewhat arbitrary; it's what is working for each of my
  // sensors. These should be tunables.
  uint16_t raw1 = analogRead(pin_sensor1);
  bool tmpState1 = raw1 >= 2800;
  rawsensor1.input(tmpState1);
  uint16_t raw2 = analogRead(pin_sensor2);
  bool tmpState2 = raw2 >= 2800;
  rawsensor2.input(tmpState2);

  bool raw1out = rawsensor1.output();
  bool raw2out = rawsensor2.output();
  
  s1out = washerSensor.input(raw1out);
  s2out = dryerSensor.input(raw2out);

  static bool prevs1;
  if (tmpState1 != prevs1) {
    lastS1Change = millis();

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
    lastS2Change = millis();
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
}

void button_loop()
{
  // If we're delaying before re-alterting or playing the tune, and
  // the button's pressed, then abort; that's confirmation that alert
  // was received
  /*
  if (washerLSM.isAlerting() && buttonIsPressed()) {
    washerLSM.buttonPressed();
    // Shut off the LEDs and delay a short time so we don't accidentally
    // trigger the sensor on our own LEDs
    //    digitalWrite(ALERT_LED, LOW);
    //    digitalWrite(SENSOR_LED, LOW);
    delay(200);
    return; // abandon this run - do not update lsm's sensor readings
  }
  */
}


void loop()
{
  server.loop();
  wifi.loop();
  tlog.loop();

  // Track the alerting state with the alert LED
  /*
  if (washerLSM.isAlerting()) {
    digitalWrite(ALERT_LED, HIGH);
    currentState = true;
  } else {
    digitalWrite(ALERT_LED, LOW);
    currentState = false;
    }*/

  if (myprefs.discordEnabled) {
    my_discord_loop();
  }

  // FIXME names of these methods -- this is being used to update homekit status
  washerCallback(washerSensor.getCurrentAnalysisAsString() == "on");
  dryerCallback(dryerSensor.getCurrentAnalysisAsString() == "on");

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

void handleRestart()
{
  ESP.restart();
}

int sendNotification(const char *url, String msg)
{
  if (!url[0]) {
    Serial.println("Unable to send notification; URL not configured");
    return 503;
  }

  String targetGetURL = url + String("/") + msg;

  tlog.logmsg(targetGetURL);
  
  HTTPClient http;
  http.begin(targetGetURL);
  int httpResponseCode = http.GET();
  http.end();
  
  return httpResponseCode;
}

void washerCallback(bool state)
{
  static bool didOnce = false;
  static bool lastState = false;
  if (!didOnce || (state != lastState)) {
    tlog.logmsg(String("Washer: ") + String(state ? "true" : "false"));
    sendNotification(myprefs.washerNotificationURL, String(state ? "1" : "0"));
    didOnce = true;
    lastState = state;
  }
}

void dryerCallback(bool state)
{
  static bool didOnce = false;
  static bool lastState = false;
  if (!didOnce || (state != lastState)) {
    tlog.logmsg(String("Dryer: ") + String(state ? "true" : "false"));
    sendNotification(myprefs.dryerNotificationURL, String(state ? "1" : "0"));
    didOnce = true;
    lastState = state;
  }
}
