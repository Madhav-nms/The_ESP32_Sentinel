# The_ESP32_Sentinel
### Edge to Cloud IoT Motion Detection Firmware

ESP32 Sentinel is firmware that acts as middleware between physical hardware and the cloud, bridging a PIR motion sensor on an ESP32 microcontroller to AWS IoT Core over a secure, authenticated MQTT connection. 
Every time motion detected, a timestamped JSON event travels from a hardware interrupt through a FreeRTOS task queue, across a mutually authenticated TLS connection and into AWS IoT Core in real time. The device also supports remote firmware updates triggered entirely from the cloud, with automatic rollback if the update fails.

## Hardware 

![Motion Detected - LED active](hw_active.png)

LED triggers in real time whem PIR sensor detects motion 

![Hardware setup](hw_setup.png)

| Component     | Pin    | Notes                        |
|---------------|--------|------------------------------|
| PIR Sensor    | GPIO 4 | HC-SR501, 3.3V or 5V VCC    |
| Onboard LED   | GPIO 2 | Mirrors PIR state            |
| GND           | GND    | Shared between ESP32 and PIR |

## System Architecture
 
```
PIR Sensor (GPIO 4)
      │
      │  Hardware interrupt fires on motion
      ▼
IRAM_ATTR ISR
      │  Timestamps event, posts to queue automically
      ▼
16-deep ISR-safe FreeRTOS Queue
      │  Decouples interrupt from network I/O
      ▼
comm_task (FreeRTOS Task)
      │  Waits for WiFi + MQTT connectivity
      │  Formats JSON payload
      ▼
AWS IoT Core (MQTT over TLS 1.2, port 8883)
      │
      ├──▶  Real-time monitoring (MQTT Test Client)
      └──▶  OTA command channel (cloud → device)
                  │
                  ▼
            OTA Task (dedicated)
            Downloads firmware from S3
            Flashes new partition
            Reboots → rollback if unhealthy
```
