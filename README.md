# RollerShutter_Arduino_MQTT_Home_Assistant (Ethernet)
https://github.com/damsma/RollerShutter_Arduino_MQTT_Home_Assistant

forked from RollerShutterSplit https://github.com/gryzli133/RollerShutterSplit

v1.0 - RollerShutter_Arduino_MQTT_Home_Assistant (Ethernet)

v1.0:
  - MQTT for Home Assistant with Auto-Discovery
  - Fixed initial relay state
  - Added tilt functionality
  - Added watchdog

Roller Shutter code - multiple Files

Library for controlling multiple roller shutters (or blinds) from a single Arduino board (ideally an Arduino MEGA) and a W5100/W5500 LAN module by defining each shutter with just one line of configuration. It utilizes MQTT with Home Assistant (HA) autodiscovery to integrate seamlessly with Home Assistant, managing button inputs, relay outputs, etc.

# To Use the code:
- download newest .ino file and all other files to one folder - same name as newest .ino file
- change the declaration in .ino file
- warning: don't use servicePin or serviceLedPin !
- for example:

{0,  21, 41, 61,        A0,  A1,  MP_PIN_NONE,         6,  7,         false,                 20, 19, 0, 50, 0, "Roleta 1"},

{1,  22, 42, 62,        A2,  A3,  MP_PIN_NONE,         8,  9,         false,                 20, 19, 0, 50, 0, "Roleta 2"},

{2,  23, 43, 63,        A4,  A5,  MP_PIN_NONE,        10, 11,         true,                  75, 68, 3, 50, 0, "Żaluzja 1"},

{4,  25, 45, 65,        A8,  A9,  MP_PIN_NONE,        14, 15,         true,                  75, 68, 3, 50, 0, "Żaluzja 3"},

{5,  26, 46, 66,        A10, A11, MP_PIN_NONE,        16, 17,         true,                  75, 68, 3, 50, 0, "Żaluzja 4"},

{6,  27, 47, 67,        A12, A13, MP_PIN_NONE,        18, 19,         true,                  75, 68, 3, 50, 0, "Żaluzja 5"},

{7,  28, 48, 68,        38,  39,  MP_PIN_NONE,        20, 21,         true,                  32, 27, 3, 50, 0, "Żaluzja 6"},

{8,  30, 50, 70,        42,  43,  MP_PIN_NONE,        24, 25,         true,                  32, 27, 3, 50, 0, "Żaluzja 7"},

{9,  31, 51, 71,        44,  45,  MP_PIN_NONE,        26, 27,         true,                  75, 68, 3, 50, 0, "Żaluzja 8"},

{10, 32, 52, 72,        46,  47,  MP_PIN_NONE,        28, 29,         true,                  75, 68, 3, 50, 0, "Żaluzja 9"},

{11, 33, 53, 73,        A15, 49,  MP_PIN_NONE,        30, 31,         true,                  75, 68, 3, 50, 0, "Żaluzja 10"},

{13, 35, 55, 75,        0,    1,  MP_PIN_NONE,        34, 35,         true,                  75, 68, 3, 50, 0, "Żaluzja 12"},

{14, 36, 56, 76,        4,    5,  MP_PIN_NONE,        36, 37,         true,                  75, 68, 3, 50, 0, "Żaluzja 13"},

{15, 29, 49, 69,        40,  41,  MP_PIN_NONE,        22, 23,         true,                  75, 68, 3, 50, 0, "Żaluzja 14"}

The values are representing:
1. Child ID of your Blind/Roller Shutter (Home Assistant will use this in entitiy name)
2. Child ID of the timer device UP (it will be not used by the controller)
3. Child ID of the timer device DOWN (it will be not used by the controller)
4. Child ID - it will be not used by the controller, but is needed to keep the times during Arduino update - KEEP ALL Child ID UNIQUE !!!
5. Pin number of the Button UP
6. Pin number of the Button DOWN
7. Pin number of the Button TOGGLE
8. Pin number of the Relay UP
9. Pin number of the Relay DOWN
10. Tilt function active (true/false)
11. Time in second for Blind to go full UP from bottom position
12. Time in second for Blind to go full DOWN from top position
13. Calibration time - this time is defining how long the relay is going to be ON after the Blind reach the UP or DOWN position - there are some situation, when the Blinds cannot reach the end position (f.e.: Time is set too short, Power supply is missing while Arduino is working etc). In most cases the best possibility is to set it to te bigger value of UP/DOWN time preset.
14. Debounce time - if you don't know what it is, just set it to 50 ms
15. It defines what state of the output the relays needs to switch ON. High Level Trigger = 0; Low level Trigger = 1
16. Description - your blind will show this description to your controller
