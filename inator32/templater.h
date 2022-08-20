#ifndef __TEMPLATER_H
#define __TEMPLATER_H

#include <Arduino.h>
#include <WebServer.h>
#include <FS.h>

typedef struct _repvars {
  String name;
  String value;
  struct _repvars *next;
} repvars;

class Templater {
 public:
  Templater();
  ~Templater();

  repvars *newRepvar(String name, String value);
  repvars *newRepvar(const char *name, const char *value);
  repvars *addRepvar(repvars *r, const char *name, const char *value);
  repvars *addRepvar(repvars *r, String name, String value);
  void deleteRepvar(repvars *r);

  void generateOutput(WebServer *server, fs::File f, repvars *r);
};

#endif
