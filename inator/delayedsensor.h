#ifndef __DELAYEDSENSOR_H
#define __DELAYEDSENSOR_H

#include <stdint.h>

/* Delayed sensor object
 *
 * Expects regular input to sensorState(). When it goes true, it
 * begins a priming sequence; if it stays true for some time
 * (PRIMINGTIMEMILLIS), then it finally enters a primed state. If it
 * goes false at any time while priming, then it becomes un-primed and
 * has to start over.
 */

#define PRIMINGTIMEMILLIS 10000

class DelayedSensor {
 public:
  DelayedSensor();
  ~DelayedSensor();

  void reset();

  void sensorState(bool isOn);

  bool isPrimed();
  bool isPriming();

 private:
  uint32_t sensorStartedPrimingAt;
  uint32_t sensorPrimedAt;
  bool sensorPrimed;
};

#endif
