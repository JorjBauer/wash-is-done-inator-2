#ifndef __MUSICPLAYER_H
#define __MUSICPLAYER_H

#include <Arduino.h>
#include <avr/pgmspace.h>

class MusicPlayer {
 public:
  MusicPlayer();
  ~MusicPlayer();

  void reset();
  void start();
  void stop();
  bool toneMaint();

  void setDelay(uint32_t delay);

  void disableAmp();
  void enableAmp();

  uint32_t loadNextEvent();


 private:
  bool isPlaying;

  uint32_t playingDelay; // configured delay (in milliseconds) between plays
  uint32_t startPlayingAt; // what millis() we should start playing at
};

#endif
