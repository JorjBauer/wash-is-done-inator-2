#ifndef __DELAYEDSENSOR_H
#define __DELAYEDSENSOR_H

#include <stdint.h>

/* Delayed sensor object
 *
 * Expects regular input to sensorState(a,b).
 *
 * Has three well defined states it will return from currentState():
 *   isOff
 *   isBlinking
 *   isOn
 * ... and some edge conditions where it will return isUnknown
 * (because the light as gone on or off within the last half second).
 *
 * There are also the intermediate transition states, which last one
 * polling interval before they transition to their stable values:
 *
 *   turnedOff -> isOff
 *   turnedOn -> isOn
 *   startedBlinking -> isBlinking
 *
 * Lastly, there's the error condition
 *   isBroken
 * that should never happen.
 */

enum {
  isOff        = 0,
  isBlinking   = 1,
  isOn         = 2,
  isUnknown    = 3,
  turnedOff,
  turnedOn,
  stoppedBeingOn,
  startedBlinking,
  isBroken
};

class DelayedSensor {
 public:
  DelayedSensor();
  ~DelayedSensor();

  void reset();

  uint8_t sensorState(bool sensorIsOn);

 private:
  uint8_t currentAnalysis, previousAnalysis;
  uint32_t lastTransitionTime;
  bool lastTransitionState;
  uint8_t transitionCount;
};

#endif
