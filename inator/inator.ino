#include <Arduino.h>
#include "musicplayer.h"

MusicPlayer mp;

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
  digitalWrite(LED1, analogRead(A0) > (1023/2));
  if (!mp.maint()) {
    //    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    bool s1 = digitalRead(SENSOR1);
    bool s2 = digitalRead(SENSOR2);
    //    digitalWrite(LED1, s1);
    digitalWrite(LED2, s2);
    delay(100);  
  } else {
    //    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
  }

}
