#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>
#include <string.h>
#include <stdio.h>
extern uint32_t millis();
#endif

#include "lsm.h"

extern void logmsg(const char *msg);
extern void startMusicPlayer();
extern void stopMusicPlayer();

LSM::LSM()
{
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
bool LSM::sensorState(bool isWasherOn, bool isDryerOn)
{
  bool ret = false;
  char buf[50];

  uint8_t previousWasherState = washerState;
  uint8_t previousDryerState = dryerState;
  
  washerState = washerSensor.sensorState(isWasherOn);
  dryerState = dryerSensor.sensorState(isDryerOn);

  // We perform actions on the edge conditions:
  //   turnedOff, turnedOn, startedBlinking

  // If either the washer or dryer has the sensor light up, then stop
  // alerting (someone physically opened the door) - opening either
  // door physically cancels both alert states.
  if ((washerState == turnedOn) ||
      (dryerState == turnedOn)) {
    logmsg("stopping previous alert\n");
    stopMusicPlayer();
    ret = true;
    alertStartedAt = 0;
  }

  // If there is no current alert, *AND*
  //    the dryer has either turned off or started blinking, *AND*
  //    the dryer was prevously on (not previously 'unknown')
  //  then that's the start of an alert
  if ((!alertStartedAt) &&
      ((dryerState == turnedOff) ||
       (dryerState == startedBlinking)) &&
      previousDryerState == isOn) {
    sprintf(buf, "starting alert b/c dryer: %d\n", dryerState);
    logmsg(buf);
    stopMusicPlayer();
    startMusicPlayer();
    alertStartedAt = millis();
    ret = true;
  }

  // If there is no current alert, *AND* the washer has either turned
  //   off or started blinking after having been on, *AND* the dryer
  //   is NOT CURRENTLY RUNNING (which would be solid on) then that's
  //   the start of an alert.  (If the dryer is still running, then we
  //   defer until the dryer ends because there's nothing for us to
  //   do.)
  if ((!alertStartedAt) &&
      ((washerState == turnedOff) ||
       (washerState == startedBlinking)) &&
      previousWasherState == isOn &&
      (dryerState != isOn)) {
    sprintf(buf, "starting alert b/c washer: %d (%d)\n", washerState, dryerState);
    logmsg(buf);
    stopMusicPlayer();
    startMusicPlayer();
    alertStartedAt = millis();
    ret = true;
  }
  
  return ret;
}

void LSM::buttonPressed()
{
  // Reset everything - no alerting, no priming
  reset();
  washerSensor.reset();
  stopMusicPlayer();
}

bool LSM::isAlerting()
{
  return (alertStartedAt ? true : false);
}

void LSM::debugTrigger()
{
  // Fake a trigger for debugging purposes
  logmsg("debugTrigger starting alert state\n");
  stopMusicPlayer();
  startMusicPlayer();
  alertStartedAt = millis();
}

uint8_t LSM::lastWasherState()
{
  return washerState;
}

uint8_t LSM::lastDryerState()
{
  return dryerState;
}

