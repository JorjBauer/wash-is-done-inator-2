#include <Arduino.h>
#include "delayedsensor.h"

DelayedSensor::DelayedSensor()
{
  reset();
}

DelayedSensor::~DelayedSensor()
{
}

void DelayedSensor::reset()
{
  sensorStartedPrimingAt = sensorPrimedAt = 0;
  sensorPrimed = false;
}

void DelayedSensor::sensorState(bool isOn)
{
  if (isOn) {
    // if we are primed, and the sensor is on, there's nothing to do.
    if (sensorPrimed) {
      return;
    }

    // So, we're not primed. Are we priming?
    if (sensorStartedPrimingAt) {
      // Are we past the priming time?
      if ((uint32_t)millis() >= (uint32_t)sensorStartedPrimingAt + (uint32_t)PRIMINGTIMEMILLIS) {
	sensorPrimed = true;
	sensorPrimedAt = millis();
	return;
      }

      return;
    }

    // We're not primed, and we're not priming. Start priming.
    sensorStartedPrimingAt = millis();
  } else {
    // Are we primed? If so, let's un-prime immediately.
    if (sensorPrimed) {
      sensorPrimed = 0;
      sensorPrimedAt = 0;
      sensorStartedPrimingAt = 0;
      return;
    }

    // Are we priming? If so, stop immediately.
    if (sensorStartedPrimingAt) {
      sensorStartedPrimingAt = 0;
      return;
    }

    // Am not primed, and was not primed. Sensor must have already
    // been off; nothing to do.
  }
}

bool DelayedSensor::isPrimed()
{
  return sensorPrimed;
}

bool DelayedSensor::isPriming()
{
  return sensorStartedPrimingAt ? true : false;
}

