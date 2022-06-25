#include "musicplayer.h"

// mp3 is created by
// $ sox ../other/gravityfalls.wav -c 1 -r 44100 data/gravityfalls-mono.wav
// $ ffmpeg -i data/gravityfalls-mono.wav data/gravityfalls-mono.mp3
//
// Mono and lower bitrate to keep the CPU need down - at 44.1k and
//   stereo it really wants to cache it (it stutters) but it seems ok at 44.1k
//   mono. And the original wav is mono anyway.

MusicPlayer::MusicPlayer()
{
  file = NULL;
  wav = NULL;
  out = NULL;
  startAgainAt = 0;
}

MusicPlayer::~MusicPlayer()
{
  stop();
}

// Volume 0.05 -> 1.0
void MusicPlayer::start(float volume)
{
  stop();
  file = new AudioFileSourceSPIFFS("/gravityfalls-mono.wav");
  out = new AudioOutputI2S();
  this->volume = volume;
  out->SetGain(volume);
  wav = new AudioGeneratorWAV();
  wav->begin(file, out);
}

void MusicPlayer::stop()
{
  if (wav) {
      delete wav;
      wav = NULL;
  }
  if (out) {
    delete out;
    out = NULL;
  }
  if (file) {
    delete file;
    file = NULL;
  }
}

// external interface: stop playing and cancel any future alerts pending
void MusicPlayer::endAlert()
{
  stop();
  startAgainAt = 0;
}

bool MusicPlayer::isPlaying()
{
  return (wav && out && file && wav->isRunning());
}

// return true if still playing
bool MusicPlayer::maint()
{
  if (!wav || !out || !file) {
    if (startAgainAt &&
        (millis() > startAgainAt)) {
      startAgainAt = 0;
      start(this->volume);
      return true;
    }
    
    return false;
  }
  if (!wav->loop()) {
    stop();
    // Prep to start playing again in 7 minutes FIXME constant
    startAgainAt = millis() + 7*60*1000;
    if (startAgainAt == 0) startAgainAt = 1; // rare rollover condition...
    
    return false;
  }
  
  return true;
}
