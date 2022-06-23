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
  mp3 = NULL;
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
  file = new AudioFileSourceSPIFFS("/gravityfalls-mono.mp3");
  out = new AudioOutputI2S();
  out->SetGain(volume);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
}

void MusicPlayer::stop()
{
  if (mp3) {
      delete mp3;
      mp3 = NULL;
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
  return (mp3 && out && file && mp3->isRunning());
}

// return true if still playing
bool MusicPlayer::maint()
{
  if (!mp3 || !out || !file)
    return false;
  if (!mp3->loop()) {
    stop();
    return false;
  }
  
  return true;
}
