#include <Arduino.h>

#include "lsm.h"
#include "musicplayer.h"

extern void logmsg(const char *msg);

MusicPlayer musicPlayer;

LSM::LSM()
{
  this->volume = 0.05; // default (as quiet as can be)
  reset();
}

LSM::~LSM()
{
}

void LSM::reset()
{
  alertStartedAt = 0;
}

// return true if the inner state changes (we start or stop alerting)
bool LSM::sensorState(bool isOn)
{
  bool ret = false;

  bool beforeState = washerSensor.isPrimed();
  washerSensor.sensorState(isOn);
  bool afterState = washerSensor.isPrimed();
  bool afterPState = washerSensor.isPriming();

  static bool lastBeforeState;
  static bool lastAfterState;
  static bool lastAfterPState;
  if (beforeState != lastBeforeState) {
    if (beforeState) {
      logmsg("beforeState changed to TRUE\n");
    } else {
      logmsg("beforeState changed to false\n");
    }
    lastBeforeState = beforeState;
  }
  if (lastAfterState != afterState) {
    if (afterState) {
      logmsg("afterState changed to TRUE\n");
    } else {
      logmsg("afterState changed to false\n");
    }
    lastAfterState = afterState;
  }
  
  if (!beforeState && (afterState || afterPState)) {
    // Was off before; but is primed (or priming) now! That stops alerting, if it was
    if (alertStartedAt) {
      logmsg("stopping previous alert\n");
      musicPlayer.stop();
      ret = true;
      alertStartedAt = 0;
    }
  }
  if (beforeState && !afterState) {
    // Was on before; but is off now! That's "start alerting"
    logmsg("starting alert\n");
    musicPlayer.start(volume);
    alertStartedAt = millis();
    ret = true;
  }

  musicPlayer.maint();
  return ret;
}

void LSM::trigger()
{
  // for debugging purposes - set up the alerting state and go
  if (alertStartedAt) {
    logmsg("stopping previous alert\n");
    musicPlayer.stop();
    alertStartedAt = 0;
  }
  
  logmsg("starting manually triggered alert\n");
  musicPlayer.start(volume);
  alertStartedAt = millis();
  
}

void LSM::buttonPressed()
{
  // Reset everything - no alerting, no priming
  reset();
  washerSensor.reset();
  musicPlayer.stop();
}

void LSM::setTimer(uint32_t ms)
{
  reset();
  washerSensor.reset();
  musicPlayer.stop();

  //  musicPlayer.setDelay(ms);
  musicPlayer.start(volume);
}

bool LSM::isAlerting()
{
  return (alertStartedAt ? true : false);
}

void LSM::setVolume(float v)
{
  this->volume = v;
}
