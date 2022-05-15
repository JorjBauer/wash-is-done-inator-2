#include <Arduino.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"


AudioGeneratorMP3 *mp3;
AudioFileSourceSPIFFS *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

// mp3 is created by
// $ sox ../other/gravityfalls.wav -c 1 -r 44100 data/gravityfalls-mono.wav
// $ ffmpeg -i data/gravityfalls-mono.wav data/gravityfalls-mono.mp3
//
// Mono and lower bitrate to keep the CPU need down - at 44.1k and
//   stereo it really wants to cache it (it stutters) but it seems ok at 44.1k
//   mono. And the original wav is mono anyway.

void setup()
{
  Serial.begin(115200);
  delay(1000);
  SPIFFS.begin();
  Serial.println("Starting");
  
  file = new AudioFileSourceSPIFFS("/gravityfalls-mono.mp3");
  id3 = new AudioFileSourceID3(file);
  out = new AudioOutputI2S();
  out->SetGain(0.1);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  Serial.printf("FreeHeap: %d\n",ESP.getFreeHeap());
}

void loop()
{
 if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.printf("MP3 done\n");
    delay(1000);
  }
}
