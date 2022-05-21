#include <Arduino.h>
#include "musicplayer.h"
#include "delayedsensor.h"

MusicPlayer mp;
DelayedSensor s1, s2;

#define LED1 1 // TX pin, GPIO 1
#define LED2 4 // D2 pin, GPIO 4

#define SENSOR1 16 // D0 pin, GPIO 16
#define SENSOR2 5 // D1 pin, GPIO 5


void setup()
{
  delay(1000);
  SPIFFS.begin();

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  pinMode(SENSOR1, INPUT);
  pinMode(SENSOR2, INPUT);

  pinMode(A0, INPUT);
  
  mp.start();
}

void loop()
{
  //  digitalWrite(LED1, analogRead(A0) > (1023/2));

  s1.sensorState(digitalRead(SENSOR1));
  s2.sensorState(digitalRead(SENSOR2));

  digitalWrite(LED1, s1.isPrimed());
  digitalWrite(LED2, s2.isPrimed());
  
  if (!mp.maint()) {
    delay(100);  
  }

}
