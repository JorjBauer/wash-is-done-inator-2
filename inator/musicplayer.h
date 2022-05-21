#ifndef __MUSICPLAYER_H
#define __MUSICPLAYER_H

#include <Arduino.h>
#include "AudioFileSourceSPIFFS.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

class MusicPlayer {
 public:
  MusicPlayer();
  ~MusicPlayer();

  void start();
  void stop();
  bool maint();

 private:
  AudioGeneratorMP3 *mp3;
  AudioFileSourceSPIFFS *file;
  AudioOutputI2S *out;
  AudioFileSourceID3 *id3;
};

#endif
