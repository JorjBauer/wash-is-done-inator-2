#ifndef __DEBOUNCE_H
#define __DEBOUNCE_H

#include <Arduino.h>

class Debounce {
 public:
  Debounce();
  ~Debounce();
  
  void input(bool state);
  bool output();

 protected:
  void _checkTimer();

 private:
  uint32_t debounce_start;
  bool last_state;
  bool debounced_state;
  bool am_debouncing;
};


#endif
