#include <Arduino.h>
#include "musicplayer.h"

MusicPlayer musicPlayer;

uint32_t nextTransmitTime = 0;

void setup() {
  musicPlayer.start();
}

void loop() {
  musicPlayer.toneMaint();
}
