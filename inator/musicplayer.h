#ifndef __MUSICPLAYER_H
#define __MUSICPLAYER_H

#include <Arduino.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

class MusicPlayer {
 public:
  MusicPlayer();
  ~MusicPlayer();

  void start(float volume);
  void stop();
  bool maint();

  void endAlert();

  bool isPlaying();

 private:
  AudioGeneratorWAV *wav;
  AudioFileSourceSPIFFS *file;
  AudioOutputI2S *out;

  uint32_t startAgainAt;

  float volume;
};

#endif
