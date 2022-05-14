#include <Arduino.h>
#include <synth_4922.h>
#include "musicplayer.h"

#include "globalpins.h"

typedef struct _note {
  uint16_t timeSincePrevious;
  uint8_t track;
  uint16_t midiNote;
  uint32_t duration;
} note_t;

#include "gravity-falls-seq.h"
//#include "doctor-who-seq.h"
//#include "muppets-seq.h"

extern uint32_t nextTransmitTime; // FIXME This is terrible as an extern global

synth edgar;

volatile uint32_t nextMidiEventAt;
volatile uint16_t midiPtr;
volatile uint16_t stopAt[4];

//#define DEFAULTDELAY (7L * 60L * 1000L) // 7 minutes
#define DEFAULTDELAY 0

MusicPlayer::MusicPlayer()
{
  playingDelay = DEFAULTDELAY;

  reset();
  
}

MusicPlayer::~MusicPlayer()
{
}

void MusicPlayer::reset()
{
  isPlaying = false;

  nextMidiEventAt = 0;
  midiPtr = 0;
  for (int i=0; i<4; i++) {
    stopAt[i] = 0;
  }
  
  startPlayingAt = 0;
  
  disableAmp();
}

void MusicPlayer::start()
{
  reset();

  startPlayingAt = (uint32_t)millis() + (uint32_t)playingDelay;
}

void MusicPlayer::stop()
{
  reset(); // also disables amp
}

void MusicPlayer::enableAmp()
{
  pinMode(SHDN, INPUT); // floating input enables the amp

  edgar.begin(DIFF); // CHA: pin 11; CHB: pin 3; DIFF: both

//setupVoice( voice[0-3] , waveform[SINE,TRIANGLE,SQUARE,SAW,RAMP,NOISE] , 
// pitch[0-127], envelope[ENVELOPE0-ENVELOPE3], length[0-127], mod[0-127, 64=no mod])

#if 1
  /* Voices for Gravity Falls theme */
  edgar.setupVoice(0,SINE,64,ENVELOPE0,80,64); // plunk
  edgar.setupVoice(1,SAW,64,ENVELOPE3,100,64); // sustained
  edgar.setupVoice(2, NOISE, 64, ENVELOPE2,60,64); // bassy snare
  edgar.setupVoice(3,NOISE,0,ENVELOPE3,60,64); // tappy snare
#endif

#if 0
  /* Voices for the Doctor Who theme */
  edgar.setupVoice(0,SAW,60,ENVELOPE0,100,64);
  edgar.setupVoice(1,SAW,60,ENVELOPE2,100,64);
  edgar.setupVoice(2,SAW,60,ENVELOPE1,100,64);
  edgar.setupVoice(3,SINE,60,ENVELOPE0,100,64); // unused
#endif

#if 0
  /* Voices for Muppets theme */
  edgar.setupVoice(0,SINE,64,ENVELOPE0,100,64); // melody
  edgar.setupVoice(1,SINE,64,ENVELOPE2,100,64); // harmony
  edgar.setupVoice(2,SINE,64,ENVELOPE0,100,64); // bass
  edgar.setupVoice(3,NOISE,0,ENVELOPE3,60,64); // drums
#endif
  
  //  edgar.setupVoice(2,SINE,64,ENVELOPE2,110,64);
//  edgar.setupVoice(3,SINE,67,ENVELOPE0,110,64); // longer sustained
//  edgar.setupVoice(3,NOISE,0,ENVELOPE3,20,64); // tappy snare
//  edgar.setupVoice(3, SQUARE, 60, ENVELOPE0, 10, 64);
}

void MusicPlayer::disableAmp()
{
  pinMode(SHDN, OUTPUT);
  digitalWrite(SHDN, 0); // ground == amp disabled
}

void MusicPlayer::setDelay(uint32_t delay)
{
  playingDelay = delay;
}

uint32_t MusicPlayer::loadNextEvent()
{
  if (midiPtr >= NUMDATA) {
    // End of song - stop, and then re-schedule starting
    stop();
    //    playingDelay = DEFAULTDELAY; // reset the delay to the default
    playingDelay = 7L*60L*1000L;
    nextTransmitTime = 0; // transmit an update ASAP
    start();
    return 0; // song stopped - FIXME - how to return this better?
  }
  
  // ... pick up the next bits of the midi.
  uint16_t timeSincePrevious = pgm_read_word(&mididata[midiPtr].timeSincePrevious);
  uint8_t track = pgm_read_byte(&mididata[midiPtr].track);
  uint16_t midiNote = pgm_read_word(&mididata[midiPtr].midiNote);
  uint32_t duration = pgm_read_word(&mididata[midiPtr].duration);

  // Find the time of the next event so we know when we need to trigger again
  uint16_t timeTillNext = duration; // If we're at the end, then it's whatever the max duration is of anything in gflight. This is kinda a problem, FIXME - there's no guarantee it will be the duration of the last note read, need to spin through duration[] & see what's remaining
  if (midiPtr < NUMDATA-1) {
    timeTillNext = pgm_read_word(&mididata[midiPtr+1].timeSincePrevious);
  }

  if (track < 4) {
    while (midiNote < 13+23) {midiNote += 12;} // transpose up an octave b/c it plays poorly so deep
    edgar.mTrigger(track, midiNote);
    // Umm, probably want edgar.setLength(track, ?); -- FIXME
    stopAt[track] = duration;
  }
  midiPtr++;

  return timeTillNext * TEMPO;
}
  

bool MusicPlayer::toneMaint()
{
  if (startPlayingAt) {
    if (millis() >= startPlayingAt) {
      // Time has come to start playing! Let it run...
      Serial.println("starting to play");
      startPlayingAt = 0;
      isPlaying = true;
      enableAmp();

      nextTransmitTime = 0; // transmit an update ASAP

      nextMidiEventAt = 0; // force immediate update
    } else {
      // Still waiting to start
      return false;
    }
  }

  if (!isPlaying)
    return false;

  if (millis() >= nextMidiEventAt) {
    nextMidiEventAt = millis() + loadNextEvent();

    /* FIXME this would actually play something, yes? That's definitely not what we want
    for (int i=0; i<4; i++) {
      if (millis() >= stopAt[i]) {
	edgar.mTrigger(i, 0);
      }
      }*/
  }
  return true; // song still playing
}
