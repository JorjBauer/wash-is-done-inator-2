#ifndef __WASHPREFS_H
#define __WASHPREFS_H

#include <Arduino.h>
#include "Prefs.h"

struct WashPrefs : public Prefs {
 public: WashPrefs();
  virtual ~WashPrefs();

  virtual void extendedWrite(fs::File f);

  virtual void setDefaults();
  virtual void set(const char *what, String newVal);
  virtual void set(const char *what, const char *newVal);

 public:
  bool discordEnabled;
  char discordURL[150];
  char washerNotificationURL[150];
  char dryerNotificationURL[150];
  char messageURL[150];
};

#endif
