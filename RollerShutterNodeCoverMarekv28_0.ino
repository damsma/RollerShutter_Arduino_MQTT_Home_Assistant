/*
https://github.com/damsma/RollerShutter_Arduino_MQTT_Home_Assistant
forked from https://github.com/gryzli133/RollerShutterSplit

v1.0 - RollerShutter_Arduino_MQTT_Home_Assistant (Ethernet)

v1.0:
  - MQTT for Home Assistant with Auto-Discovery
  - Fixed initial relay state
  - Added tilt functionality
  - Added watchdog
*/
#define VERSION F("v1.0 - RollerShutter_Arduino_MQTT_Home_Assistant (Ethernet)")

#include "NetworkConfig.h" // for W5500 LAN gateway

#include <Watchdog.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

#include "RollerShutter.h"

IPAddress ip(MYIPADDR);
IPAddress dns(MYDNS);
IPAddress gw(MYGW);
IPAddress sn(MYIPMASK);
EthernetClient ethClient;
PubSubClient client(ethClient);

/****************************************RollerShutter objects***************************************/

/* define your RollerShutter objects here
 RollerShutter(int childId      // Cover/Roller Shutter device ID for MySensors and Controller (Domoticz, HA etc)   - also used as EEPROM address - must be unique !
              , int setIdUp     // Roll time UP setpoint device ID for MySensors and Controller (Domoticz, HA etc)  - also used as EEPROM address - must be unique !
              , int setIdDown   // Roll time DOWN setpointdevice ID for MySensors and Controller (Domoticz, HA etc) - also used as EEPROM address - must be unique !
              , int initId      // Initialization device ID for MySensors and Controller (Domoticz, HA etc)         - also used as EEPROM address - must be unique !
              , int buttonUp    // Button Pin for UP
              , int buttonDown  // Button Pin for DOWN
              , int buttonToggle// Button Pin to TOGGLE (UP/STOP/DOWN)
              , int relayUp     // Relay Pin for UP
              , int relayDown   // Relay Pin for DOWN
              , bool canTilt    // true or false
              , uint8_t initTimeUp          // Initial Roll time UP
              , uint8_t initTimeDown        // Initial Roll time DOWN
              , uint8_t initCalibrationTime // Initial calibration time (time that relay stay ON after reach 0 or 100%)
              , int debaunceTime            // Time to debounce button -> standard = 50
              , bool invertedRelay          // for High level trigger = 0; for Low level trigger = 1
              )
*/

bool service = 0;
int servicePin = 2;
int serviceLedPin = 13; // built-in LED
uint32_t lastCycle;
uint32_t minCycle = 0;
uint32_t maxCycle = 60000;
int cycleCount=0;
int newLevel = 0;
uint32_t lastAllUp;

unsigned long previousEthernetTime = 0;

static bool initial_state_sent = false;

// Enable thermostat object to change the time from Domoticz/HA
//#define MP_BLINDS_TIME_THERMOSTAT

/*
{1,       21,        41,          61,          22,       23,         MP_PIN_NONE,  38,      39,        true,    21,         20,           21,                  50,           0,             "Roleta w kuchni"},

(childId, childIdUp, childIdDown, childIdInit, buttonUp, buttonDown, buttonToggle, relayUp, relayDown, canTilt, initTimeUp, initTimeDown, initCalibrationTime, debaunceTime, invertedRelay, Info)
*/
RollerShutter blinds[] =
{
//
// warning: don't use servicePin or serviceLedPin !
//

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
};

const int blindsCount = sizeof(blinds) / sizeof(RollerShutter);

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("Starting"));

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // START WATCHDOG
  pinMode(LED_BUILTIN, OUTPUT);
  wdt_enable(WDTO_8S);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  digitalWrite(LED_BUILTIN, LOW);
  delay(50);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);
  // START WATCHDOG END

  setup_ethernet();

  // RESET WATCHDOG TIMER
  wdt_reset();
  
  maintain_ethernet();
  
  // RESET WATCHDOG TIMER
  wdt_reset();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setBufferSize(JSON_BUFFER_SIZE_OUT);
  client.setCallback(callback);

  Serial.println("Started");
  // Service Mode input = D2 -> only if pass-through mode is needed, f.e. for programming the build-in motor's limit switches.
  pinMode(servicePin, INPUT_PULLUP);
  service = !digitalRead(servicePin);
  pinMode(serviceLedPin, OUTPUT);
  digitalWrite(serviceLedPin, service);
  
  // Setup locally attached sensors
  for(int i = 0; i < blindsCount; i++)
  {
    // RESET WATCHDOG TIMER
    wdt_reset();
    if(service)
    {
      blinds[i].enterServiceMode();
    }
    else
    {
      blinds[i].publishDiscoveryConfig();
    }
  }

  Serial.println(F("Ready"));
  digitalWrite(LED_BUILTIN, HIGH);
}

void presentController()
{
  for (int i = 0; i < blindsCount; i++) {
    blinds[i].publishDiscoveryConfig();

    client.subscribe(blinds[i].commandTopic().c_str());
    client.subscribe(blinds[i].positionSetTopic().c_str());
    client.subscribe(blinds[i].tiltCommandTopic().c_str());

    client.publish(blinds[i].availabilityTopic().c_str(), "online", true);
  }

  for(int i = 0; i < blindsCount; i++)
  {
    blinds[i].SyncController();
  }
}

void loop()
{
  unsigned long nowTime = millis();

  if (!initial_state_sent) {
    presentController();
    
    initial_state_sent = true;
  }

  // RESET WATCHDOG TIMER
  wdt_reset();

  if (nowTime - previousEthernetTime >= MAINTAIN_ETHERNET_INTERVAL) {
    previousEthernetTime = nowTime;
    maintain_ethernet();
  }
  
  // RESET WATCHDOG TIMER
  wdt_reset();
  
  if (!client.connected()) {
    reconnect();
  }

  client.loop(); //MQTT check

  for(int i = 0; i < blindsCount; i++)
  {
    blinds[i].Update();
  }

  #ifdef MP_DEBUG_CYCLE_TIME
  if(cycleCount>1000)
  {
    Serial.println(String(minCycle));
    Serial.println(String(maxCycle));
    cycleCount = 0;
    minCycle = 60000;
    maxCycle = 0;
  }
  minCycle = min(millis() - lastCycle , minCycle);
  maxCycle = max(millis() - lastCycle , maxCycle);
  cycleCount++;
  lastCycle = millis();
  #endif
}

/* --------------------------- */
// MQTT handling
/* --------------------------- */

void callback(char *topic, byte *payload, unsigned int length) {
  payload[length] = '\0';

  String topicStr = String(topic);
  String payloadStr = String((char *)payload);

  Serial.println(F("----------------------------------"));
  Serial.println(F("Incoming MQTT message"));
  Serial.print(F("Topic: "));
  Serial.println(topicStr);
  Serial.print(F("Payload: "));
  Serial.println(payloadStr);
  Serial.println(F("----------------------------------"));

  bool handled = false;

  for (int i = 0; i < blindsCount; i++) {
    String cmdTopic = blinds[i].commandTopic();
    String posTopic = blinds[i].positionSetTopic();

    if (topicStr == cmdTopic || topicStr == posTopic) {
      Serial.print(F("Message for blind #"));
      Serial.print(i);
      Serial.print(F(" ("));
      Serial.print(blinds[i].getRelayDescription());
      Serial.println(F(")"));

      blinds[i].handleCommand(topic, (char *)payload);
      handled = true;
      break;
    }

    if (topicStr == blinds[i].tiltCommandTopic()) {
      Serial.print(F("Message for blind #"));
      Serial.print(i);
      Serial.print(F(" ("));
      Serial.print(blinds[i].getRelayDescription());
      Serial.println(F(")"));
      int tiltValue = atoi((char*)payload);
      blinds[i].handleTilt(tiltValue);
      handled = true;
      break;
    }

  }

  if (!handled) {
    Serial.println(F("No blind matched for this topic."));
  }
}

/* --------------------------- */
// Network handling
/* --------------------------- */

bool parseIP(const char* ipStr, IPAddress& ipOut) {
  int a, b, c, d;
  if (sscanf(ipStr, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
    return false;
  }
  ipOut = IPAddress(a, b, c, d);
  return true;
}

bool pingMQTTServer() {
  EthernetClient pingClient;
  IPAddress mqttIP;

  if (!parseIP(MQTT_SERVER, mqttIP)) {
    Serial.println(F("Bad formatted MQTT_SERVER"));
    return false;
  }

  if (pingClient.connect(mqttIP, 1883)) {
    pingClient.stop();
    return true;
  } else {
    Serial.println(F("Failed connecting to MQTT broker"));
    pingClient.stop();
    return false;
  }
}

bool isNetworkConfigured() {
  IPAddress ip = Ethernet.localIP();
  return (ip[0] != 0 && ip[0] != 255);
}

void setup_ethernet() {  
  #if defined(Pico_with_w5500)
    Ethernet.init(17); // Raspberry Pi Pico with w5500
  #endif

  #if defined(Easyswitch_with_w5500)
    Ethernet.init(48); // Easyswitch with w5500
  #endif

  #if defined(Standard_w5500)
    Ethernet.init(); // Pin 10
  #endif

  Ethernet.begin(mac, ip, dns, gw, sn);
  Serial.println(F("LAN SETUP:"));

  Serial.print(F("Local IP : "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("Subnet Mask : "));
  Serial.println(Ethernet.subnetMask());
  Serial.print(F("Gateway IP : "));
  Serial.println(Ethernet.gatewayIP());
  Serial.print(F("DNS Server : "));
  Serial.println(Ethernet.dnsServerIP());
}

void maintain_ethernet() {
  uint8_t nTries = 0;

  while (!isNetworkConfigured()) {
    Serial.println(F("No IP - retrying Ethernet configuration.."));
    setup_ethernet();

    nTries++;
    //reset arduino if not connected after 5th time
    if(nTries <= 5) {
      // RESET WATCHDOG TIMER
      wdt_reset();
    }
  }

  while (!pingMQTTServer()) {
    Serial.println(F("Pinging MQTT server failed - restarting ethernet.."));
    setup_ethernet();

    nTries++;
    //reset arduino if not connected after 5th time
    if(nTries <= 5) {
      // RESET WATCHDOG TIMER
      wdt_reset();
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print(F("Connecting to MQTT broker.."));
    if (client.connect(MQTT_DEVICE_NAME, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(F("connected"));
      
      delay(200);

      presentController();
    } else {
      Serial.print(F("failed with state: "));
      Serial.print(client.state());
      Serial.println(F("trying again in 5s"));
      delay(5000);
    }
  }
}