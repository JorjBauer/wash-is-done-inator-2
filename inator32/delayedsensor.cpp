#ifdef ARDUINO
#include <Arduino.h>
#include "delayedsensor.h"
#else
#include "delayedsensor.h"
extern uint32_t millis();
#endif

// How many ms before we consider it "compeltely on" or "completely
// off" rather than blinking? In theory, 1000 (1s) is fine; but
// sometimes the homekit crypto takes > 1s, and we miss a polling
// cycle. So trying 5s...
#define HOLDTIME 5000

extern void logmsg(const char *msg);

DelayedSensor::DelayedSensor()
{
  reset();
}

DelayedSensor::~DelayedSensor()
{
}

void DelayedSensor::reset()
{
  currentAnalysis = isUnknown;
  previousAnalysis = isUnknown;
  lastTransitionTime = 0;
  transitionCount = 0;
  lastTransitionState = false;
}

uint8_t DelayedSensor::sensorState(bool sensorIsOn)
{
  // First handle the case where we've decided it's full-on, because
  // that's easy. Either it's still on or it is off (and goes to an
  // unknown state that might be off or might be blinking)
  if (currentAnalysis == isOn) {
    if (sensorIsOn) {
      // Nothing to do if we've decided it's solidly on, and it's still on
      return currentAnalysis;
    } else {
      // Transition from on-to-off - we can't tell if it's off or blinking yet
      previousAnalysis = currentAnalysis;
      currentAnalysis = isUnknown;
      lastTransitionTime = millis();
      transitionCount = 1;
      lastTransitionState = sensorIsOn;
      logmsg("sensor update: on-to-unknown\n");
      return previousAnalysis; // Let the Unknown handler return a state in the next update
    }
  }

  // Similarly easy: if we decided it's off, then either it's still
  // off or it went to unknown
  if (currentAnalysis == isOff) {
    if (!sensorIsOn) {
      return currentAnalysis;
    } else {
      // Transition from off-to-on - we can't tell if it's off or blinking yet
      previousAnalysis = currentAnalysis;
      currentAnalysis = isUnknown;
      lastTransitionTime = millis();
      transitionCount = 1;
      lastTransitionState = sensorIsOn;
      logmsg("sensor update: off-to-unknown\n");
      return previousAnalysis; // Let the Unknown handler return a state in the next update
    }
  }

  // If we've decided it's blinking, it could transition to off or on,
  // or still be blinking. But it's not unknown in any of those states.
  char buf[50];
  if (currentAnalysis == isBlinking) {
    if (sensorIsOn != lastTransitionState) {
      lastTransitionState = sensorIsOn;
      // The sensor has changed state. How long has it been?
      if ((millis() - lastTransitionTime) < HOLDTIME) {
        // Less than a second. It's still blinking.
        transitionCount++;
        sprintf(buf, "sensor update: blinking, still: %ld\n", millis()-lastTransitionTime);
        logmsg(buf);
        lastTransitionTime = millis();
        return currentAnalysis;
      } else {
        // It stayed that way for more than a second before changing -
        // that's not blinking, it's either on or off
        previousAnalysis = currentAnalysis;
        currentAnalysis = sensorIsOn ? isOn : isOff;
        sprintf(buf, "sensor update: blinking-to-%s: %ld\n", sensorIsOn?"on":"off", millis()-lastTransitionTime);
        logmsg(buf);
        lastTransitionTime = millis();
        transitionCount = 1;
        return sensorIsOn ? turnedOn : turnedOff;
      }
    } else {
      // Sensor state is the same as it was before - how long has it
      // been that way?
      if ((millis() - lastTransitionTime) >= HOLDTIME) {
        // More than a second: it's no longer blinking
        previousAnalysis = currentAnalysis;
        currentAnalysis = sensorIsOn ? isOn : isOff;
        lastTransitionTime = millis();
        transitionCount = 1;
        lastTransitionState = sensorIsOn;
        lastTransitionTime = millis();
        if (sensorIsOn)
          logmsg("sensor update: blinking-to-on\n");
        else
          logmsg("sensor update: blinking-to-off\n");
        return sensorIsOn ? turnedOn : turnedOff;
      } else {
        // still blinking, nothing to do
        return currentAnalysis;
      }
    }
  }

  // If we don't know what state it's in, then we need to watch for
  // transitions.
  if (currentAnalysis == isUnknown) {
    // Either the light has transitioned, or it has not.
    if (sensorIsOn == lastTransitionState) {
      // Has it been in the same state for more than a second?
      if ((millis() - lastTransitionTime) >= HOLDTIME) {
        // Yes: so we know it's either on or off now
        //
        // Don't change previousAnalysis - we need to know if this is
        // an on-unknown-off or just unknown-off
        currentAnalysis = sensorIsOn ? isOn : isOff;
        transitionCount = 1;
        lastTransitionState = sensorIsOn;
        lastTransitionTime = millis();
        if (sensorIsOn)
          logmsg("sensor update: unknown-to-on\n");
        else
          logmsg("sensor update: unknown-to-off\n");
        return sensorIsOn ? turnedOn : turnedOff;
      } else {
        // No: less than a second means we still don't know
        // anything. Keep waiting for a transition or for more than a
        // second.
        return previousAnalysis;
      }
    } else {
      // Okay, a transition happened. Was it after more than a second?
      if ((millis() - lastTransitionTime) >= HOLDTIME) {
        // This is a weird case. We didn't get any sensor updates for
        // a second, but then we got a transition notice after a
        // second. So we skipped a state of "definintely on" or
        // "definitely off" and went to the following state of "was on
        // but now it's unknown again". Restart the timers and see if
        // we can guess better next time?
        //
        // Don't change previousAnalysis - we need to know if this is
        // an on-unknown-off or just unknown-off
        currentAnalysis = sensorIsOn ? isOn : isOff;
        transitionCount = 1;
        lastTransitionState = sensorIsOn;
        lastTransitionTime = millis();
        return sensorIsOn ? turnedOn : turnedOff;
      }
      // A transition at less than a second could be a blink, if there
      // have been enough of them. Let's count!
      if (++transitionCount >= 4) {
        // YES it's definitely blinking.
        previousAnalysis = currentAnalysis;
        currentAnalysis = isBlinking;
        lastTransitionState = sensorIsOn;
        lastTransitionTime = millis();
        logmsg("sensor update: unknown-to-blinking\n");
        return startedBlinking;
      }
      // No, we're still looking for more blinks to confirm, so keep waiting
      lastTransitionState = sensorIsOn;
      lastTransitionTime = millis();
      logmsg("sensor update: waiting for more blinks\n");
      return previousAnalysis;
    }
  }

  // NOTREACHED (error, unknown state)
  return isBroken;
}
