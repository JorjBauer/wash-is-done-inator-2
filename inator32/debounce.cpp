#include "debounce.h"

#define DEBOUNCEINTERVAL 100
// Should we debounce when doing off-to-on, or both off-to-on and on-to-off?
// If this is '1', then only do off-to-on
#define DEBOUNCE_ON_ONLY 1

Debounce::Debounce()
{
  debounced_state = last_state = false;
  debounce_start = 0;
  am_debouncing = false;
}

Debounce::~Debounce()
{
}

void Debounce::_checkTimer()
{
  // If we're waiting for a debounce pulse to settle, and that time
  // has elapsed, then consider it done.
  if (am_debouncing && !last_state && DEBOUNCE_ON_ONLY) {
    // If we're only doing DEBOUNCE_ON_ONLY, and the new state would be off,
    // and we think we're debouncing... then we're not debouncing. We're done.
    debounced_state = last_state;
    am_debouncing = false;
  } else if (am_debouncing && millis() - debounce_start >= DEBOUNCEINTERVAL) {
    debounced_state = last_state;
    am_debouncing = false;
  }
}

void Debounce::input(bool state)
{
  _checkTimer();

  if (last_state != state) {
    last_state = state;

    // There are still three possibilities here.
    //   1. We're still debouncing, but the timer hasn't expired.
    //   2. We're not debouncing, and the input is the same as the last state.
    //   3. We're not debouncing, and the input is different than the last state.
    // In all of these cases, we just want to restart the debouncer.
    // Technically, we could weed out #3 (and presumably #1, which is probably 
    // "we're debouncing, timer hasn't expired, and now the input has changed")
    // and just set am_debouncing = false for those two cases; but the effect 
    // is the same as telling the debouncer that it's waiting for a timer to 
    // expire before it "changes" the value back to the current value...
    
    am_debouncing = true;
    debounce_start = millis();
  } else {
    last_state = state;
    // ... but don't change debounce
  }
}

bool Debounce::output()
{
  _checkTimer();

  return debounced_state;
}

