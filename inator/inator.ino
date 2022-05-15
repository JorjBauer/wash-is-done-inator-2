#include <Arduino.h>
#include <ESP8266Wavplay.h>

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.print("Starting");
  wavSetup();
}


// $ sox ../other/gravityfalls.wav -c 1 -r 20000 data/gravityfalls-mono.wav
// single channel is important. Tested bitrates from 8k to 20k, all fine. 20k
// obviously sounds best.
void loop()
{
  if (!wavPlaying()) {
    delay(1000);
    showDir();
    setGain(0.1);
    wavStartPlaying("/gravityfalls-mono.wav");
  }
  wavLoop();
}
