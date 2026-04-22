# The_ESP32_Sentinel
### Edge to Cloud IoT Motion Detection Firmware

ESP32 Sentinel is firmware that acts as middleware between physical hardware and the cloud, bridging a PIR motion sensor on an ESP32 microcontroller to AWS IoT Core over a secure, authenticated MQTT connection. 
Every time motion detected, a timestamped JSON event travels from a hardware interrupt through a FreeRTOS task queue, across a mutually authenticated TLS connection and into AWS IoT Core in real time. The device also supports remote firmware updates triggered entirely from the cloud, with automatic rollback if the update fails.

## Hardware 

(docs/hw_active.png)

