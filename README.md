This is an Arduino IDE-built project on a WEMOS D1. I used this one (assuming it still exists):

  http://amazon.com/gp/product/B07PF3NK12

The project is documented over here:

  https://hackaday.io/project/43887-the-wash-is-done-inator


Prep
===

Tools -> Board -> Boards Manager

  make sure that the esp8266 boards are installed. I'm using package version 3.0.2.

Install required libraries

* HomeKit-ESP8266 (v1.2.0) - installed via Tools -> Manage Libraries
* ESP8266Audio (v1.9.5) - installed via Tools -> Manage Libraries


Build Settings
===

Board: "LOLIN(WEMOS) D1 R2 & mini"
Upload Speed: "921600"
CPU Frequency: "160 MHz"
Flash Size: "4MB (FS:2MB OTA:~1019KB)"
Debug port: "Disabled"
Debug Level: "None"
IwIP Variant: "v2 Higher Bandwidth"
VTables: "Flash"
C++ Exceptions: "Disabled (new aborts on oom)"
Stack Protection: "Disabled"
Erase Flash: "Only Sketch"
SSL Support: "All SSL ciphers (most compatible)"
MMU: "16KB cache + 48KB IRAM (IRAM)"
Non-32-Bit Access: "Use pgm_read macros for IRAM/PROGMEM"

The app fits in the default MMU 32KB cache + 32KB IRAM, but that's not
how I'm running it. Similarly, the program is only about ???KB/MB; the
Flash/SPIFFS trade-off could be 3MB instead of 2MB and it should be
okay.

SPIFFS
===

The files in the data/ directory have to be installed in SPIFFS. I've
done that with "Tools -> ESP8266 Sketch Data Upload" (using v0.5.0
from
https://github.com/esp8266/arduino-esp8266fs-plugin/releases). Yes,
SPIFFS is deprecated, and I'm using it anyway; I had performance
problems with LittleFS, and I'll come back around to that some other
day.


HomeKit
===

The HomeKit layer often has trouble with pairing (but seems fine once
it's paired). It looks like 3 of 4 attempts to "Add Accessory" work;
you may just have to try multiple times. Make sure that the build
settings set the IwIP variant to "v2 Higher Bandwidth" and the CPU set
to 160MHz or it will definitely fail.

Generally speaking, HomeKit seems pretty obnoxious in its device
rigidity. There is no "Wash Done Sensor", and no real way to define
one. I've picked a Contact Sensor protocol (#10) and use the
open/closed settings ("open" being used for when the device is
alerting, and "closed" for when it is in its normal state).

