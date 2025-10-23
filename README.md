# Disclaimer

This code is provided "as is", without warranty of any kind. It has not been fully tested and may contain bugs or unintended behavior. Use at your own risk.

The author is not responsible for any damage or loss caused by the use of this code or hardware setup.


# RollerShutter_Arduino_MQTT_Home_Assistant (Ethernet)
https://github.com/damsma/RollerShutter_Arduino_MQTT_Home_Assistant

forked from RollerShutterSplit https://github.com/gryzli133/RollerShutterSplit

v1.1 - RollerShutter_Arduino_MQTT_Home_Assistant (Ethernet)

v1.1:
  - Fixed closing would not work when tilted manually (whend stopped manually with button under 1%)

v1.0:
  - MQTT for Home Assistant with Auto-Discovery
  - Fixed initial relay state
  - Added tilt functionality
  - Added watchdog

Roller Shutter code - multiple Files

Library for controlling multiple roller shutters (or blinds) from a single Arduino board (ideally an Arduino MEGA) and a W5100/W5500 LAN module by defining each shutter with just one line of configuration. It utilizes MQTT with Home Assistant (HA) autodiscovery to integrate seamlessly with Home Assistant, managing button inputs, relay outputs, etc.

# Screenshots

In Home Assistant each blind will look like this (canTilt is set to true):
<img width="614" height="658" alt="grafik" src="https://github.com/user-attachments/assets/4ac2d869-e73a-4fac-8a33-42aa025331d8" />

Roller shutters will look like this (canTilt is set to false):
<img width="621" height="667" alt="grafik" src="https://github.com/user-attachments/assets/719dede4-b89c-4c6a-b865-b8c842a803be" />

# How to use the code
1. Download newest .ino file and all other files to one folder - same name as newest .ino file.
2. Install Arduino IDE
3. Install Libraries in Arduino IDE:
     - Watchdog 3.0.2 by Peter Polidoro
     - Ethernet 2.0.0 by Arduino
     - PubSubClient 2.8.0 by Nick O'Leary
     - ArduinoJson 6.21.5 by Benoît Blanchon
     - Bounce2 2.53.0 by Thomas O Fredericks

## define your blinds / roller shutters
Change the declaration in RollerShutter_Arduino_MQTT_Home_Assistant.ino file.

#### Warning: don't use the same pin as the one defined in servicePin or serviceLedPin variables!

For example:
```
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
```

The values are representing:
1. Child ID of your blind / roller shutter (Home Assistant will use this in entitiy name)
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

## define your network settings
Open NetworkConfig.h and change the values to match your network.

```
// Set the static IP address to use if the DHCP fails to assign
#define MYIPADDR 192,168,1,199
#define MYIPMASK 255,255,255,0
#define MYDNS 192,168,1,1
#define MYGW 192,168,1,1

byte mac[] = {0x00, 0x10, 0xF1, 0x6E, 0x01, 0x99};
```

Define the MQTT broker address/credentials

```
#define MQTT_SERVER "192.168.1.188"
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_PORT 1883
```

Depending on your hardware you may choose one line by uncommenting it, this will change the pin of the network adapter:

```
//#define Standard_w5500 1
#define Easyswitch_with_w5500 1
//#define Pico_with_w5500 1
```

## upload your code to your Arduino Mega 2560

Make sure you check if everything works fine before connecting to your blinds / roller shutters by checking each relay state in different conditions.

# Other requirements

## Software
You may need a working installation of Home Assistant (open source home automation system), as well as Mosquitto (open source MQTT broker).

## Hardware
Tested on Arduino Mega 2560 with W5500.
W5100 will also work.

The code is prepared for the Wiznet W5500-EVB-Pico, but I have not tested it yet on this project. There's also the voltage difference (5V vs 3.3V) that needs to be considered and the custm board definition has to be installed in the Arduino IDE.

You may need some sort of a din board, for example https://sklep.easyswitch.pl/produkt/es-1-04-atmega-din-board-ethernet-w5500/ 

And some relays, i use Relpol SSR (5v) + din mount.
