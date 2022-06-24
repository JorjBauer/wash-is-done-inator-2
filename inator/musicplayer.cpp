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

bool MusicPlayer::isPlaying()
{
  return (wav && out && file && wav->isRunning());
}

// return true if still playing
bool MusicPlayer::maint()
{
  if (!wav || !out || !file)
    return false;
  if (!wav->loop()) {
    stop();
    return false;
  }
  
  return true;
}
