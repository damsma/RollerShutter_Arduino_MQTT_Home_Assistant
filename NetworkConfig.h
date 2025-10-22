#ifndef _AUTH_DETAILS
#define _AUTH_DETAILS

#define MQTT_BASE_TOPIC "homeassistant/cover"
#define MQTT_DEVICE_NAME "RollerShutter"

#define Standard_w5500 1
//#define Easyswitch_with_w5500 1
//#define Pico_with_w5500 1

#define MAINTAIN_ETHERNET_INTERVAL 120000

// Set the static IP address to use if the DHCP fails to assign
#define MYIPADDR 192,168,1,199
#define MYIPMASK 255,255,255,0
#define MYDNS 192,168,1,1
#define MYGW 192,168,1,1

byte mac[] = {0x00, 0x10, 0xF1, 0x6E, 0x01, 0x99};

// Set the IP address/details of the MQTT broker
#define MQTT_SERVER "192.168.1.188"
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_PORT 1883

#endif
