#ifndef __LSM_H
#define __LSM_H

#include <stdint.h>
#include "delayedsensor.h"

/* Laundry State Machine!
 *
 * The LSM takes input from the light sensor and a single button. Its
 * output is one boolean - whether or not it is currently alerting.
 *
 * The sensor has to be on for a certain amount of time - and once
 * it's good, then it "primes".
 *
 * When it's primed, and then un-primes, then we've been
 * triggered. That's when we set of an alert.
 */

class LSM {
 public:
  LSM();
  ~LSM();

  void reset();

  bool sensorState(bool s);
  void buttonPressed();

  bool isAlerting();
  uint8_t lastState();
  
  void debugTrigger();
  
  // private:
  DelayedSensor sensor;
  uint8_t state;
  
  uint32_t alertStartedAt;
};

#endif
