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

#include <EEPROM.h>
#include <ArduinoJson.h> 
#include <Bounce2.h>

// 100% is opened - up
// 0% is closed - down
#define MP_MIN_SYNC 2000        // Send data to controller not often than every 2s
#define MP_MAX_SYNC 1800000     // Refresh data to controller at least once every 30min
#define MP_PIN_NONE 255         // if there is no need for PIN, then it will be used
#define MP_WAIT_DIRECTION 100   // waiting time before changing direction - to protect the motor 

/* --------------------------- */
// JSON handling
/* --------------------------- */
const unsigned int JSON_BUFFER_SIZE_IN = 512; //incoming MQTT
StaticJsonDocument<JSON_BUFFER_SIZE_IN> docIn;

const unsigned int JSON_BUFFER_SIZE_OUT = 1024; //outgoing MQTT
StaticJsonDocument<JSON_BUFFER_SIZE_OUT> docOut;

extern PubSubClient client;

bool IS_ACK = false; //is to acknowlage

class RollerShutter
{ 
  uint8_t CHILD_ID_COVER;
  uint8_t CHILD_ID_SET_UP;
  uint8_t CHILD_ID_SET_DOWN;
  uint8_t CHILD_ID_INITIALIZATION;
  uint8_t buttonPinUp;
  uint8_t buttonPinDown;
  uint8_t buttonPinToggle;
  uint8_t relayPinUp;
  uint8_t relayPinDown;
  bool CAN_TILT;
  bool relayON;
  bool relayOFF;
  bool directionUp;
  const char * relayDescription;
  uint32_t lastSync;
  uint8_t requestSync;

  bool setupMode = false;   // true = setup mode activated -> pass-through mode, Button write directly to relay.

  bool value = 0; // debauncer helper
  
  uint8_t rollTimeUp;
  uint8_t rollTimeDown;

  uint8_t currentTiltValue = 0;
  uint8_t targetTiltValue = 0;

  uint8_t requestShutterLevel;
  uint8_t currentShutterLevel;
  uint32_t currentMsUp = 0;
  uint32_t currentMsDown = 0;
  int32_t currentMs = 0;
  uint32_t timeRelayOff;
  uint32_t timeRelayOn;
  uint8_t calibrationTime;
  uint8_t relayState = 0; // 0= request relay off; 1= request relay up; 2= request relay down;
  uint8_t requestRelayState = 0; // 0= relay off; 1= relay up is on; 2= relay down is on;

  unsigned long tiltStartTime = 0;
  uint8_t tiltPhase = 0;
  bool tiltActive = false;

  const uint16_t fullTiltTime = 1200;
  const uint16_t preTiltDelay = 1200;

  bool serviceMode = 0;

  Bounce debouncerUp = Bounce();
  Bounce debouncerDown = Bounce();
  Bounce debouncerToggle = Bounce();

  bool oldValueUp = 0;
  bool oldValueDown = 0;
  bool oldValueToggle = 0;

  public:
  const char* getRelayDescription() const {
    return relayDescription;
  }
  
  public:
  RollerShutter(uint8_t childId, uint8_t setIdUp, uint8_t setIdDown, uint8_t initId,
                uint8_t buttonUp, uint8_t buttonDown, uint8_t buttonToggle, uint8_t relayUp, uint8_t relayDown,
                bool canTilt,
                uint8_t initTimeUp, uint8_t initTimeDown, uint8_t initCalibrationTime,
                int debaunceTime, bool invertedRelay, const char *descr)
  {
    relayPinUp = relayUp;
    pinMode(relayPinUp, OUTPUT);            // Then set relay pins in output mode
    digitalWrite(relayPinUp, relayOFF);     // Make sure relays are off when starting up

    relayPinDown = relayDown;    
    pinMode(relayPinDown, OUTPUT);          // Then set relay pins in output mode    
    digitalWrite(relayPinDown, relayOFF);   // Make sure relays are off when starting up

    CAN_TILT = canTilt;
    CHILD_ID_COVER = childId;
    CHILD_ID_SET_UP = setIdUp;
    CHILD_ID_SET_DOWN = setIdDown;
    CHILD_ID_INITIALIZATION = initId;
    buttonPinUp = buttonUp;
    buttonPinDown = buttonDown;
    buttonPinToggle = buttonToggle;
    relayON = !invertedRelay;
    relayOFF = invertedRelay;
    initTimeUp = initTimeUp;
    initTimeDown = initTimeDown;
    calibrationTime = initCalibrationTime;
    relayDescription = descr;
    
    if(buttonPinUp != MP_PIN_NONE) 
    {
      pinMode(buttonPinUp, INPUT_PULLUP);     // Setup the button and Activate internal pull-up
      debouncerUp.attach(buttonPinUp);        // After setting up the button, setup debouncer
      debouncerUp.interval(debaunceTime);     // After setting up the button, setup debouncer
    }
    if(buttonPinDown != MP_PIN_NONE) 
    {    
      pinMode(buttonPinDown, INPUT_PULLUP);     // Setup the button and Activate internal pull-up
      debouncerDown.attach(buttonPinDown);      // After setting up the button, setup debouncer
      debouncerDown.interval(debaunceTime);     // After setting up the button, setup debouncer
    }
    if(buttonPinToggle != MP_PIN_NONE) 
    {
      pinMode(buttonPinToggle, INPUT_PULLUP);     // Setup the button and Activate internal pull-up
      debouncerToggle.attach(buttonPinToggle);    // After setting up the button, setup debouncer
      debouncerToggle.interval(debaunceTime);     // After setting up the button, setup debouncer
    }

    if (CHILD_ID_COVER + CHILD_ID_SET_UP + CHILD_ID_SET_DOWN + CHILD_ID_INITIALIZATION + initTimeUp + initTimeDown + calibrationTime != loadState(CHILD_ID_INITIALIZATION))    
    {
        saveState(CHILD_ID_SET_UP, initTimeUp);
        saveState(CHILD_ID_SET_DOWN, initTimeDown);
        saveState(CHILD_ID_INITIALIZATION, CHILD_ID_COVER + CHILD_ID_SET_UP + CHILD_ID_SET_DOWN + CHILD_ID_INITIALIZATION + initTimeUp + initTimeDown + calibrationTime);                  
    }
    
    rollTimeUp = loadState(CHILD_ID_SET_UP);
    rollTimeDown = loadState(CHILD_ID_SET_DOWN);
    
    currentShutterLevel = loadState(CHILD_ID_COVER);
    requestShutterLevel = currentShutterLevel;
    currentMsUp = (uint32_t)10 * currentShutterLevel * rollTimeUp;

    #ifdef MP_DEBUG_SHUTTER
    Serial.print("currentShutterLevel / requestShutterLevel / currentMsUp / rollTimeUp / rollTimeDown : ");
    Serial.print(currentShutterLevel);
    Serial.print(" / ");
    Serial.print(requestShutterLevel);
    Serial.print(" / ");
    Serial.print(currentMsUp);
    Serial.print(" / ");
    Serial.print(rollTimeDown);
    Serial.print(" / ");
    Serial.println(rollTimeDown);
    #endif
  }

  String baseTopic() { return String(MQTT_BASE_TOPIC) + String("/roleta_") + String(CHILD_ID_COVER); }
  String stateTopic() { return baseTopic() + "/state"; }
  String commandTopic() { return baseTopic() + "/set"; }
  String positionTopic() { return baseTopic() + "/position"; }
  String positionSetTopic() { return baseTopic() + "/position/set"; }
  String availabilityTopic() { return baseTopic() + "/availability"; }
  String discoveryTopic() { return baseTopic() + "/config"; }
  String tiltCommandTopic() { return baseTopic() + "/tilt/set"; }
  String tiltStateTopic() { return baseTopic() + "/tilt/state"; }

  void publishDiscoveryConfig()
  {
    docOut.clear();
    JsonObject root = docOut.to<JsonObject>();
    root["name"] = relayDescription;
    root["unique_id"] = String("roleta_") + String(CHILD_ID_COVER);
    root["device_class"] = "shutter";
    root["command_topic"] = commandTopic();
    root["state_topic"] = stateTopic();
    root["availability_topic"] = availabilityTopic();
    root["payload_open"] = "OPEN";
    root["payload_close"] = "CLOSE";
    root["payload_stop"] = "STOP";
    root["set_position_topic"] = positionSetTopic();
    root["position_topic"] = positionTopic();
    root["optimistic"] = false;

    if(CAN_TILT) {
      root["tilt_command_topic"] = tiltCommandTopic();
      root["tilt_status_topic"] = tiltStateTopic();
      root["tilt_min"] = 0;
      root["tilt_max"] = 100;
      root["tilt_closed_value"] = 0;
      root["tilt_opened_value"] = 100;
    }

    char buffer[JSON_BUFFER_SIZE_OUT-1];
    serializeJson(root, buffer);
    client.publish(discoveryTopic().c_str(), buffer, true);
    client.publish(availabilityTopic().c_str(), "online", true);
  }
  
  void handleCommand(const char *topic, const char *payload)
  {
    Serial.print("handleCommand called with topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(payload);
  
    if (strcmp(topic, commandTopic().c_str()) == 0)
    {
      if (strcmp(payload, "OPEN") == 0) shuttersUp();
      else if (strcmp(payload, "CLOSE") == 0) shuttersDown();
      else if (strcmp(payload, "STOP") == 0) shuttersHalt();
    }
    else if (strcmp(topic, positionSetTopic().c_str()) == 0)
    {
      int level = atoi(payload);
      changeShuttersLevel(level);
    }
    else if (strcmp(topic, tiltCommandTopic().c_str()) == 0)
    {
      int tiltValue = constrain(atoi(payload), 0, 100);
      handleTilt(tiltValue);
    }
  }
  
  uint8_t loadState(uint8_t addr) {
    uint8_t value;
    EEPROM.get(addr, value);
    if (value > 200) value = 20;
    return value;
  }
  
  void saveState(uint8_t addr, uint8_t value) {
    EEPROM.put(addr, value);
  }
  
  void sendState()
  {
    const char* stateStr = "stopped";
    
    // RESET WATCHDOG TIMER
    wdt_reset();
  
    if(serviceMode == 0)
    {
      if(requestRelayState == 1) // 0= request relay off; 1= request relay up; 2= request relay down;
      {
        stateStr = "opening";
      }
      else 
      {
        if(requestRelayState == 2)
        {
          stateStr = "closing";
        }
        else
        {
          if(requestRelayState == 0)
          {
            stateStr = "stopped";
            if(currentShutterLevel == 100)
            {
              stateStr = "open";
            }
            if(currentShutterLevel == 0)
            {
              stateStr = "closed";
            }
          }
        }
      }

      Serial.print(F("[MQTT] Publishing state string: "));
      Serial.println(stateStr);
      Serial.print(F("[MQTT] To topic: "));
      Serial.println(stateTopic().c_str());

      client.publish(stateTopic().c_str(), stateStr, true);

      if(CAN_TILT) {
        client.publish(tiltStateTopic().c_str(), String(currentTiltValue).c_str(), true);
      }
      char posBuffer[10];
      itoa(currentShutterLevel, posBuffer, 10);
      client.publish(positionTopic().c_str(), posBuffer, true);

      requestSync ++;
      lastSync = millis();
    }
  }
  #ifdef MP_BLINDS_TIME_THERMOSTAT
  void sendTimeState()
  {
      send(msgSetpointUp.set((int)rollTimeUp));
      send(msgActualUp.set((int)rollTimeUp));
      send(msgSetpointDown.set((int)rollTimeDown));
      send(msgActualDown.set((int)rollTimeDown));        
  }
  #endif
  
  void shuttersUp(void) 
  {
    #ifdef MP_DEBUG_SHUTTER
    Serial.println("shuttersUp");
    #endif
    if(serviceMode == 0)
    {
      requestShutterLevel = 100;
    }
  }

  void shuttersDown(void) 
  {
    #ifdef MP_DEBUG_SHUTTER
    Serial.println("shuttersDown");
    #endif
    if(serviceMode == 0)
    {    
      requestShutterLevel = 0;
    }
  }    

  void shuttersHalt(void) 
  {
    #ifdef MP_DEBUG_SHUTTER
    Serial.println("shuttersHalt");
    #endif
    if(serviceMode == 0)
    {        
      requestShutterLevel = 255;
    }
  }
    
  void changeShuttersLevel(int level) 
  {
    #ifdef MP_DEBUG_SHUTTER
    Serial.println("changeShuttersLevel");
    #endif
    if(level == 255)
    {
      requestShutterLevel = 255;
    }
    else
    {
      requestShutterLevel = constrain(level, 0, 100);
    }
  }

  void handleTilt(int tiltValue)
  {
    targetTiltValue = tiltValue;
    Serial.print(F("Received tilt command: "));
    Serial.println(tiltValue);
    requestSync = 1;
  }

  void enterServiceMode()
  {
      serviceMode = 1;
  }

  void exitServiceMode()
  {
    serviceMode = 0;
  }

  void Update()
  {
    // RESET WATCHDOG TIMER
    wdt_reset();
  
    if(serviceMode == 0)
    {
      if(buttonPinUp != MP_PIN_NONE) 
      {
        debouncerUp.update();
        value = debouncerUp.read();
        if (value == 0 && value != oldValueUp) {
          if(requestRelayState != 0){
            requestShutterLevel = 255; // request stop
          }  
          else{
          requestShutterLevel = 100;
          }
          #ifdef MP_DEBUG_SHUTTER
          Serial.print("Button Up : ");
          Serial.println(requestShutterLevel);
          #endif
        }
        oldValueUp = value;
      }
      if(buttonPinDown != MP_PIN_NONE) 
      {
        debouncerDown.update();
        value = debouncerDown.read();
        if (value == 0 && value != oldValueDown) {
          if(requestRelayState != 0){
            requestShutterLevel = 255; // request stop
          }  
          else{
          requestShutterLevel = 0;
          } 
          #ifdef MP_DEBUG_SHUTTER
          Serial.print("Button Down : ");
          Serial.println(requestShutterLevel);
          #endif   
        }
        oldValueDown = value;
      }
      if(buttonPinToggle != MP_PIN_NONE)
      {
        
        debouncerToggle.update();
        value = debouncerToggle.read();
        if (value == 0 && value != oldValueToggle) 
        {
          if(requestRelayState != 0)
          {
            requestShutterLevel = 255; // request stop
          }  
          else
          {
            if(directionUp) // last direction was UP -> change to DOWN
            {
              requestShutterLevel = 0;
            }
            else // last direction was DOWN -> change to UP
            {
              requestShutterLevel = 100;
            }
          }
          #ifdef MP_DEBUG_SHUTTER
          Serial.print("Button Toggle : ");
          Serial.println(requestShutterLevel);
          #endif
        }
        oldValueToggle = value;
      }


      
      switch(relayState)
      {
        case 0:
          currentMs = currentMsUp / rollTimeUp - currentMsDown / rollTimeDown;
          break;
        case 1:
          currentMs = (currentMsUp + millis() - timeRelayOn) / rollTimeUp - currentMsDown / rollTimeDown;
          break;
        case 2:
          currentMs = currentMsUp / rollTimeUp - (currentMsDown + millis() - timeRelayOn) / rollTimeDown;
          break;
      }
      
      uint16_t currentShutterLevelTemp = constrain(currentMs, 0, 1000);
      currentShutterLevel = currentShutterLevelTemp / 10; // convert to percentage
      #ifdef MP_DEBUG_SHUTTER
      Serial.print("currentShutterLevel / requestShutterLevel / currentMs / currentMsUp / currentMsDown : ");
      Serial.print(currentShutterLevel);
      Serial.print(" / ");
      Serial.print(requestShutterLevel);
      Serial.print(" / ");
      Serial.print(currentMs);
      Serial.print(" / ");
      Serial.print(currentMsUp);
      Serial.print(" / ");
      Serial.println(currentMsDown);
      #endif

      // REQUEST LEVEL CODE //
      if(requestShutterLevel == 255)
      {
        requestRelayState = 0; 
        if(currentMs <= 0)
        {
          currentMsUp = 0;
          currentMsDown = 0;
        }
        else if(currentMs >= 1000)
        {
          currentMsUp = (uint32_t)1000 * rollTimeUp;
          currentMsDown = 0;
        }
        #ifdef MP_DEBUG_SHUTTER
        Serial.print("requestShutterLevel == 255 : ");
        Serial.print(currentShutterLevel);
        Serial.print(" / ");
        Serial.print(requestShutterLevel);
        Serial.print(" / ");
        Serial.print(currentMsUp);
        Serial.print(" / ");
        Serial.println(currentMsDown);
        #endif
      }
      else if(tiltActive || (requestShutterLevel == currentShutterLevel))
      {
        if (!tiltActive) {
          if(CAN_TILT && (targetTiltValue != currentTiltValue))
          {
            Serial.print("tiltActive!");
            tiltActive = true;
            tiltPhase = 0;
            tiltStartTime = millis();

            if(requestRelayState == 1) { //last direction was UP
              currentTiltValue = 100;
            }
            else if(requestRelayState == 2) { //last direction was DOWN
              currentTiltValue = 0;
            }

            requestRelayState = 2; //DOWN
            #ifdef MP_DEBUG_SHUTTER
              Serial.print(F("[TILT] Phase 0: close"));
              Serial.print(preTiltDelay);
              Serial.println(F(" ms"));
            #endif
          }
          else {
            if(requestShutterLevel == 0)
            {
              #ifdef MP_DEBUG_SHUTTER
              Serial.print("requestShutterLevel == 0 : ");
              Serial.print(currentShutterLevel);
              Serial.print(" / ");
              Serial.print(requestShutterLevel);
              Serial.print(" / ");
              Serial.print(currentMsUp);
              Serial.print(" / ");
              Serial.println(currentMsDown);
              #endif
              if(currentMs <= (int32_t)0 - ((int32_t)1000 * calibrationTime / rollTimeDown))
              {
                if(CAN_TILT) {
                  currentTiltValue = 0;
                  targetTiltValue = 0;
                }
                requestRelayState = 0;
                currentMsUp = 0;
                currentMsDown = 0;
                currentTiltValue = 0;
              }
            }
            else if(requestShutterLevel == 100)
            {
              #ifdef MP_DEBUG_SHUTTER
              Serial.print("requestShutterLevel == 100 : ");
              Serial.print(currentShutterLevel);
              Serial.print(" / ");
              Serial.print(requestShutterLevel);
              Serial.print(" / ");
              Serial.print(currentMsUp);
              Serial.print(" / ");
              Serial.println(currentMsDown);
              #endif
              if(currentMs >= (int32_t)1000 + ((int32_t)1000 * calibrationTime / rollTimeUp))
              {
                if(CAN_TILT) {
                  currentTiltValue = 100;
                  targetTiltValue = 100;
                }
                requestRelayState = 0; 
                currentMsUp = (uint32_t)1000 * rollTimeUp;
                currentMsDown = 0;
              }
            }
            else
            {
              #ifdef MP_DEBUG_SHUTTER
              Serial.print("requestShutterLevel == currentShutterLevel : ");
              Serial.print(currentShutterLevel);
              Serial.print(" / ");
              Serial.print(requestShutterLevel);
              Serial.print(" / ");
              Serial.print(currentMsUp);
              Serial.print(" / ");
              Serial.println(currentMsDown);
              #endif
              if(CAN_TILT) {
                if(requestRelayState == 1) { //last direction was UP
                  currentTiltValue = 100;
                  targetTiltValue = 100;
                }
                else if(requestRelayState == 2) { //last direction was DOWN
                  currentTiltValue = 0;
                  targetTiltValue = 0;
                }
              }
              requestRelayState = 0;
            }
          }
        }
        else {
          int tiltDelta = targetTiltValue;
          uint16_t moveTime = map(tiltDelta, 0, 100, 0, fullTiltTime);

          if (tiltActive && (tiltPhase == 0) && ((millis() - tiltStartTime) >= preTiltDelay))
          {
            tiltPhase = 1;
            tiltStartTime = millis();
        
            requestRelayState = 0; //STOP

            #ifdef MP_DEBUG_SHUTTER
              Serial.print(F("[TILT] Phase 1: stop for "));
              Serial.print(preTiltDelay);
              Serial.println(F(" ms"));
            #endif
          }

          if (tiltActive && (tiltPhase == 1) && ((millis() - tiltStartTime) >= preTiltDelay))
          {
            tiltPhase = 2;
            tiltStartTime = millis();
        
            requestRelayState = 1; //UP

            #ifdef MP_DEBUG_SHUTTER
              Serial.print(F("[TILT] Phase 2: UP for "));
              Serial.print(moveTime);
              Serial.println(F(" ms"));
            #endif
          }

          if (tiltActive && (tiltPhase == 2))
          {
            if ((millis() - tiltStartTime) >= moveTime)
            {
              requestRelayState = 0; //STOP
              tiltActive = false;
              tiltPhase = 0;
              currentTiltValue = targetTiltValue;

              requestShutterLevel = currentShutterLevel;

              requestSync = 1;

              #ifdef MP_DEBUG_SHUTTER
                Serial.println(F("[TILT] done"));
              #endif
            }
          }
          
        }
      }
      else
      {
        requestSync = 1;
        if(requestShutterLevel > currentShutterLevel)
        {
          #ifdef MP_DEBUG_SHUTTER
          Serial.print("requestShutterLevel > currentShutterLevel : ");
          Serial.print(currentShutterLevel);
          Serial.print(" / ");
          Serial.print(requestShutterLevel);
          Serial.print(" / ");
          Serial.print(currentMsUp);
          Serial.print(" / ");
          Serial.println(currentMsDown);
          #endif
          requestRelayState = 1;
        }
        else
        {
          #ifdef MP_DEBUG_SHUTTER
          Serial.print("requestShutterLevel < currentShutterLevel : ");
          Serial.print(currentShutterLevel);
          Serial.print(" / ");
          Serial.print(requestShutterLevel);
          Serial.print(" / ");
          Serial.print(currentMsUp);
          Serial.print(" / ");
          Serial.println(currentMsDown);
          #endif
          requestRelayState = 2;
        }
      }
      //**********************

      if(relayState != requestRelayState)
      {
        if(!tiltActive) {
          requestSync = 1;
        }
        #ifdef MP_DEBUG_SHUTTER
        Serial.print("relayState / requestRelayState; ");
        Serial.print(relayState);
        Serial.print(" / ");
        Serial.println(requestRelayState);
        #endif
        if(relayState != 0)
        {
          digitalWrite(relayPinUp, relayOFF);
          digitalWrite(relayPinDown, relayOFF);
          timeRelayOff = millis();
          
          if(relayState == 1) // UP
          {
            currentMsUp += timeRelayOff - timeRelayOn;
          } 
          else                // DOWN
          {
            currentMsDown += timeRelayOff - timeRelayOn;
          }
          saveState(CHILD_ID_COVER, currentShutterLevel);
          relayState = 0;
        }
        else
        {
          if(millis() - timeRelayOff > MP_WAIT_DIRECTION)
          {
            if(requestRelayState == 1)        //Request UP
            {
              digitalWrite(relayPinDown, relayOFF);
              digitalWrite(relayPinUp, relayON);
              relayState = 1;
              directionUp = true;
              timeRelayOn = millis();
            }
            else if(requestRelayState == 2)   //Request DOWN
            {
              digitalWrite(relayPinUp, relayOFF);
              digitalWrite(relayPinDown, relayON);
              relayState = 2;
              directionUp = false;
              timeRelayOn = millis();
            }
          }
        }
      }

      if((requestSync == 1 && millis() - lastSync > MP_MIN_SYNC) ||              // sync first time
         (requestSync == 2 && millis() - lastSync > (MP_MIN_SYNC+MP_MAX_SYNC)/4) || // resync after quarter time
         (requestSync == 3 && millis() - lastSync > (MP_MIN_SYNC+MP_MAX_SYNC)/2) || // resync after half time
         millis() - lastSync > MP_MAX_SYNC)                                      // resync every MP_MAX_SYNC time
      {
        if(!tiltActive) {
          sendState();
        }
      }
    }
    else // Service Mode Aktive = input signal connected directly to output !!!
    {
      if(buttonPinUp != MP_PIN_NONE) digitalWrite(relayPinUp, !digitalRead(buttonPinUp));
      if(buttonPinDown != MP_PIN_NONE) digitalWrite(relayPinDown, !digitalRead(buttonPinDown));
    }
  }

  void SyncController()
  {
    requestSync = 1;
  }
  
  void SetupMode(bool active)
  {
    setupMode = active;
  }  

  #ifdef MP_BLINDS_TIME_THERMOSTAT
    void Receive(const MyMessage &message)
    {
      else if (message.sensor == CHILD_ID_SET_UP) 
      {
          if (message.type == V_HVAC_SETPOINT_COOL || message.type == V_HVAC_SETPOINT_HEAT) 
          {
            #ifdef MP_DEBUG_SHUTTER
            Serial.println(", New status: V_HVAC_SETPOINT_COOL, with payload: ");
            #endif
            rollTimeUp = message.getInt();
            #ifdef MP_DEBUG_SHUTTER
            Serial.println("rolltime Up value: ");
            Serial.println(rollTimeUp);
            #endif
            saveState(CHILD_ID_SET_UP, rollTimeUp);
            requestSync = true;
          }
          sendTimeState();
      }
      else if (message.sensor == CHILD_ID_SET_DOWN) 
      {
          if (message.type == V_HVAC_SETPOINT_COOL || message.type == V_HVAC_SETPOINT_HEAT) 
          {
            #ifdef MP_DEBUG_SHUTTER
            Serial.println(", New status: V_HVAC_SETPOINT_COOL, with payload: ");
            #endif      
            rollTimeDown = message.getInt();
            #ifdef MP_DEBUG_SHUTTER
            Serial.println("rolltime Down value: ");
            Serial.println(rollTimeDown);
            #endif
            saveState(CHILD_ID_SET_DOWN, rollTimeDown);
            requestSync = true;
          }
          sendTimeState();
        }      
      #ifdef MP_DEBUG_SHUTTER
        Serial.println("exiting incoming message");
      #endif
        return;
      }
  #endif
};
