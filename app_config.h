#pragma once

// WiFi 
#define WIFI_SSID           " "    
#define WIFI_PASS           " "    

// AWS IoT Core 
#define MQTT_URI            " "       // device data endpoint
#define MQTT_CLIENT_ID      " "       // AWS IoT Thing name

// MQTT Topics
#define MQTT_TOPIC          " "       // Device → Cloud (motion events)
#define MQTT_CMD_TOPIC      " "       // Cloud → Device (OTA commands)

// GPIO Pins
#define PIR_GPIO            4       
#define LED_GPIO            2        

// FreeRTOS Event Group Bits 
#define WIFI_CONNECTED_BIT  (1 << 0)   // Set when WiFi obtains an IP address
#define MQTT_CONNECTED_BIT  (1 << 1)   // Set when MQTT broker connection is established