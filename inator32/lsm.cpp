#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>
#include <string.h>
#include <stdio.h>
extern uint32_t millis();
#endif

#include "lsm.h"

// There are two modes of operation here: first, we call an immediate sync
// callback on edge conditions (beginAlert() / endAlert()); and second,
// the caller can tell what the current state is from sensorState(). It's up
// to the caller to decide what to do with all this information.

extern void logmsg(const char *msg);
extern void beginAlert(void *o);
extern void endAlert(void *o);

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
bool LSM::sensorState(bool s)
{
  bool ret = false;
  char buf[50];

  uint8_t previousState = state;
  
  state = sensor.sensorState(s);

  // We perform actions on the edge conditions:
  //   turnedOff, turnedOn, startedBlinking

  // If either the washer or dryer has the sensor light up, then stop
  // alerting (someone physically opened the door) - opening either
  // door physically cancels both alert states.
  if (state == turnedOn) {
    logmsg("LSM is done alerting\n");
    endAlert(this);
    ret = true;
    alertStartedAt = 0;
  }

  // If there is no current alert and the sensor has gone off or
  // started blinking *BUT* was previously running, then that's
  // the start of an alert
  if ((!alertStartedAt) &&
      ((state == turnedOff) ||
       (state == startedBlinking)) &&
      previousState == isOn) {
    sprintf(buf, "starting alert b/c state %d\n", state);
    logmsg(buf);
    endAlert(this);
    beginAlert(this);
    alertStartedAt = millis();
    ret = true;
  }
  
  return ret;
}

void LSM::buttonPressed()
{
  // Reset everything - no alerting, no priming
  reset();
  sensor.reset();
  endAlert(this);
}

bool LSM::isAlerting()
{
  return (alertStartedAt ? true : false);
}

void LSM::debugTrigger()
{
  // Fake a trigger for debugging purposes
  logmsg("debugTrigger starting alert state\n");
  endAlert(this);
  beginAlert(this);
  alertStartedAt = millis();
}

uint8_t LSM::lastState()
{
  return state;
}

