#ifndef __MUSICPLAYER_H
#define __MUSICPLAYER_H

#include <Arduino.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

class MusicPlayer {
 public:
  MusicPlayer();
  ~MusicPlayer();

  void start(float volume);
  void stop();
  bool maint();

  bool isPlaying();

 private:
  AudioGeneratorMP3 *mp3;
  AudioFileSourceSPIFFS *file;
  AudioOutputI2S *out;
};

#endif
