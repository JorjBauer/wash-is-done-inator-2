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
  id3 = NULL;
  out = new AudioOutputI2S();
  out->SetGain(0.05); // debugging -- make 1.0 eventually
  mp3 = NULL;
}

MusicPlayer::~MusicPlayer()
{
  stop();
}

void MusicPlayer::start()
{
  stop();
  
  file = new AudioFileSourceSPIFFS("/gravityfalls-mono.mp3");
  id3 = new AudioFileSourceID3(file);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
}

void MusicPlayer::stop()
{
  if (mp3) {
      mp3->stop();
      delete mp3;
  }
  mp3 = NULL;
  if (id3)
    delete id3;
  id3 = NULL;
  if (file)
    delete file;
  file = NULL;
}

// return true if still playing
bool MusicPlayer::maint()
{
  if (!mp3)
    return false;
  
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      return false;
    }
    return true;
  } else {
    return false;
  }
}
