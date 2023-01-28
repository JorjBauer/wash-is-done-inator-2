#include "WashPrefs.h"

WashPrefs::WashPrefs() : Prefs()
{
}

WashPrefs::~WashPrefs()
{
}

void WashPrefs::extendedWrite(fs::File f)
{
  f.print("discordEnabled=");
  f.println(discordEnabled ? "1" : "0");
  f.print("discordURL=");
  f.println(discordURL);
  f.print("messageURL=");
  f.println(messageURL);
  f.print("washerNotificationURL=");
  f.println(washerNotificationURL);
  f.print("dryerNotificationURL=");
  f.println(dryerNotificationURL);
}

void WashPrefs::setDefaults()
{
  Prefs::setDefaults();
  discordEnabled = false;
  discordURL[0] = '\0';
  messageURL[0] = '\0';
  washerNotificationURL[0] = '\0';
  dryerNotificationURL[0] = '\0';
}

void WashPrefs::set(const char *what, String newVal)
{
  set(what, newVal.c_str());
}

void WashPrefs::set(const char *what, const char *newVal)
{
  if (!strcmp(what, "discordEnabled")) {
    discordEnabled = (newVal[0] == '1');
  } else if (!strcmp(what, "discordURL")) {
    strncpy(discordURL, (char *)newVal, sizeof(discordURL)-1);
  } else if (!strcmp(what, "messageURL")) {
    strncpy(messageURL, (char *)newVal, sizeof(messageURL)-1);
  } else if (!strcmp(what, "washerNotificationURL")) {
    strncpy(washerNotificationURL, (char *)newVal, sizeof(washerNotificationURL)-1);
  } else if (!strcmp(what, "dryerNotificationURL")) {
    strncpy(dryerNotificationURL, (char *)newVal, sizeof(dryerNotificationURL)-1);
  } else {
    Prefs::set(what, newVal);
  }
}
