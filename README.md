# ESP32 Weather Station — IoT Weather Monitoring System

A real-time weather monitoring system built on ESP32, combining physical sensors (temperature, humidity, pressure), a TFT display for visualization, NTP time sync, weather forecast data from a public API, and two-way data exchange over MQTT — all running concurrently on a **multitasking FreeRTOS architecture**. The system utilizes **Telegraf** as an agent to automatically ingest data from MQTT into **InfluxDB**, and a **Node.js web dashboard** to query and visualize the historical data.

The project consists of decoupled components:
- **`mini_iot_weather_station/`** — embedded firmware running on ESP32 (PlatformIO).
- **`telegraf.conf`** — Telegraf agent configuration for the ETL pipeline (MQTT to InfluxDB).
- **`iot_weather_dashboard/`** — Node.js backend + web dashboard, serving APIs and querying InfluxDB.
## 1. Architecture Overview

```text
┌──────────────────────────┐                 ┌──────────────────────────┐
│    ESP32 (Sensors)       │                 │       Node.js Server     │
│ - FreeRTOS Multitasking  │                 │ - Express / REST API     │
│ - Read DHT11, BMP180     │                 │ - Handle CORS            │
│ - ST7735 TFT Display     │                 │ - Serve Dashboard UI     │
└────────────┬─────────────┘                 └─────────────▲────────────┘
             │ Publish                                     │ Query via Flux
             ▼                                             │
┌──────────────────────────┐                 ┌─────────────▼────────────┐
│    Mosquitto Broker      │                 │       InfluxDB (v2)      │
│ - Local MQTT (Port 1883) │                 │ - Time-series Database   │
│ - Topic: home/sensor/data│                 │ - Persistent Storage     │
└────────────┬─────────────┘                 └─────────────▲────────────┘
             │ Subscribe                                   │
             │                                             │ HTTP Write
             │         ┌──────────────────────────┐        │
             └────────►│         Telegraf         ├────────┘
                       │ - MQTT Consumer Plugin   │
                       │ - Parse JSON & Rename    │
                       │ - Buffer & Flush (10s)   │
                       └──────────────────────────┘
```
The system runs 4 FreeRTOS tasks in parallel on the ESP32, synchronizing shared data through mutexes (Semaphores) and a queue, pinned across the chip's 2 physical cores to balance load and prevent one task from starving the others. On the backend, the data pipeline is strictly decoupled: Telegraf handles data ingestion (subscribing to MQTT and writing to InfluxDB), while the Node.js service (server.js) focuses on querying data and displaying it through a web dashboard (public/).

2. Knowledge & Skills Applied
2.1. Embedded Systems Programming
ESP32 microcontroller programming in C++ (Arduino framework) on PlatformIO, with code organized into standard PlatformIO internal libraries (lib/DisplayManager, lib/NetworkManager, lib/SensorManager — each module with its own .h/.cpp pair) instead of one monolithic .ino file — applying the separation of concerns and modular library structure principles.

Separating sensitive configuration from source code: using a secrets.h / secrets_example.h pair — secrets.h holds real WiFi passwords, MQTT credentials and API keys and is excluded via .gitignore, while secrets_example.h is a safe template committed to Git — the standard security pattern for projects under public version control.

Hardware protocol communication:

SPI (bit-banged / software SPI) driving the ST7735 TFT display (driver command set: initR, writeCommand, sleep/wake commands SLPIN/SLPOUT, DISPON/DISPOFF).

I2C for reading the BMP180 pressure sensor (via the Adafruit_BMP085_Unified library).

DHT11's proprietary 1-wire protocol (via the DHT library).

GPIO pin management on the ESP32-WROOM-32: understanding real hardware constraints — GPIO6–11 are not broken out externally (used internally for flash SPI), strapping pins (GPIO0, GPIO2, GPIO12, GPIO15) affect boot mode — leading to identifying and fixing real pin conflicts (e.g. TFT_RST and DHTPIN initially both mapped to GPIO4; the initial I2C configuration used GPIO11/12, pins that don't physically exist on the board).

2.2. Real-Time Operating System — FreeRTOS
Multitasking via xTaskCreatePinnedToCore(), assigning task priority and core affinity (pin) to balance load across the chip's two CPU cores.

Synchronizing shared data across tasks using Semaphores/Mutexes (xSemaphoreCreateMutex, xSemaphoreTake, xSemaphoreGive) to prevent race conditions when multiple tasks read/write the same global state (e.g. globalSensorData, the city variable, the tft display object).

Inter-task communication via a Queue (xQueueCreate, xQueueSend, xQueueReceive) to pass MQTT data between tasks without directly sharing memory.

Task notifications (xTaskNotifyGive, ulTaskNotifyTake) to let one task wake another on an event (e.g. receiving a city-change command over MQTT immediately wakes TaskWeatherAPI instead of waiting for its 30-minute cycle).

System resource management: sizing task stacks appropriately, and checking the return values of mutex/queue/task creation calls to avoid hard-to-debug runtime failures when the system runs low on RAM.

2.3. Networking & IoT
MQTT protocol (PubSubClient library): periodically publishing sensor data, subscribing to remote control commands (display sleep/wake, changing the tracked city), and handling asynchronous callbacks.

Deploying and configuring an MQTT broker (Mosquitto) on Windows: installing it as a Windows Service, configuring listener/allow_anonymous in mosquitto.conf, debugging connectivity with Test-NetConnection, opening inbound Windows Firewall rules, and using mosquitto_pub/mosquitto_sub to verify two-way data flow.

REST API / HTTPS client (HTTPClient, WiFiClientSecure): calling the OpenWeatherMap API, parsing the JSON response with ArduinoJson, and handling timeouts and network errors.

NTP protocol (NTPClient) for real-time clock sync over the network, including handling the not-yet-synced state (epoch time before year 2024).

Two-way JSON serialization/deserialization: packaging sensor data to publish to the broker, and decoding control commands (including nested JSON) received from the broker.

2.4. System Debugging Skills
Isolating faults with minimal test cases — writing a standalone test sketch that only initializes the display hardware (no WiFi/sensors/FreeRTOS) to determine precisely whether a fault was in software or hardware, before misattributing the cause.

Analyzing system logs (Serial Monitor) to trace exactly where code hung or crashed — adding deliberate logging ([Display] ...) at each initialization step to confirm actual execution flow instead of guessing.

Identifying and fixing concurrency bugs: finding a race condition on the shared String city variable between two tasks and fixing it with a mutex; identifying the risk of an unbounded block (portMAX_DELAY) that could stall the whole system, and replacing it with a controlled timeout.

Network/infrastructure debugging: using ipconfig and Test-NetConnection, and distinguishing the real Wi-Fi adapter IP from virtual adapter IPs (VMware) — an easy mistake to make on a dev machine with multiple network adapters.

Fixing embedded UI layout bugs: identifying drawing coordinates that exceeded the physical screen size (128×128px) and redesigning the layout to fit the small display properly.

Memory safety review: adding explicit null-termination after strncpy, and checking the return values of resource-allocating calls (mutex/queue/task) to avoid crashes from using NULL handles.

2.5. Security & Basic Operations
Identifying and addressing a security risk: separating sensitive information (API keys, WiFi passwords, MQTT credentials, InfluxDB tokens) from the source code via secrets.h, .env, and telegraf_example.conf + .gitignore instead of hardcoding it directly — avoiding exposure when pushing code to a public version control system (Git).

Configuring Windows Firewall to open a controlled port (an inbound rule for a specific port) rather than disabling the firewall entirely.

2.6. Data Pipeline, Backend & Dashboard (Telegraf + Node.js + InfluxDB)
ETL Pipeline with Telegraf: Configured the Telegraf agent (mqtt_consumer) to automatically parse incoming JSON from the MQTT broker, rename fields via processors, and safely batch-write data to InfluxDB, optimizing ingestion performance compared to manual backend processing.

Building a Node.js backend (server.js): Serving as the query and API layer, retrieving data from InfluxDB via Flux queries to serve the static web dashboard, adhering to a decoupled architecture.

Building a REST API with Express, including CORS configuration (cors) so the dashboard frontend can call the backend from a different origin/port during development.

Writing the backend using ES Modules ("type": "module" in package.json, import/export syntax) instead of legacy CommonJS (require), reflecting current Node.js conventions.

Writing a sensor simulator script (sim_sensor.js) to test the MQTT → Telegraf → InfluxDB → dashboard data flow independently, without needing real hardware — applying a testable, decoupled architecture mindset.

3. Project Structure
project esp32 weather/
│
├── mini_iot_weather_station/        # Embedded firmware (PlatformIO)
│   ├── include/
│   │   ├── config.h                 # GPIO pin config, MQTT server, WiFi SSID
│   │   ├── system_types.h           # Shared data structs + extern declarations
│   │   └── bitmaps.h                # Logo & weather icon bitmap data
│   ├── lib/                         # Internal libraries, one folder per module
│   │   ├── DisplayManager/
│   │   ├── NetworkManager/
│   │   └── SensorManager/
│   ├── src/
│   │   ├── main.cpp                 # Entry point, sets up mutexes/queues/tasks
│   │   ├── secrets.h                # Real WiFi password, MQTT credentials (gitignored)
│   │   └── secrets_example.h        # Sample template, safe to commit to Git
│   ├── platformio.ini
│   └── .gitignore
│
├── iot_weather_dashboard/           # Backend + web dashboard (Node.js)
│   ├── public/                      # Dashboard UI (static files)
│   ├── server.js                    # Subscribes to commands, queries InfluxDB, serves dashboard
│   ├── sim_sensor.js                # Sensor simulator script for standalone testing
│   ├── package.json / package-lock.json
│   └── .env                         # Environment variables (InfluxDB token, MQTT host...)
│
├── telegraf_example.conf            # Safe configuration template for the Telegraf agent
└── .gitignore                       # Root gitignore (excludes telegraf.conf)
4. Hardware Used
Component	                         Model	                       Interface
Microcontroller	               ESP32-WROOM-32 (DevKitC)	                —
Display	                       ST7735 1.44" 128×128 RGB TFT	           SPI
Temperature/Humidity sensor	        DHT11	                         1-wire
Pressure sensor	                    BMP180	                          I2C
5. Libraries & Technologies Used
Firmware (ESP32 / PlatformIO):
Adafruit_GFX, Adafruit_ST7735, Adafruit_BMP085_Unified, Adafruit_Sensor, DHT, PubSubClient, ArduinoJson, NTPClient, WiFiClientSecure, HTTPClient — all running on the ESP32 Arduino Core.

Data Pipeline & Backend (Telegraf, Node.js, ES Modules):
Telegraf (MQTT consumer, InfluxDB v2 output), express (HTTP server serving the dashboard and REST endpoints), mqtt (Node.js MQTT client for commands), @influxdata/influxdb-client (querying time-series data), dotenv (loading environment variables from .env), cors.

6. Build & Run Instructions
Firmware (ESP32):

Bash
cd mini_iot_weather_station

# Copy the secrets template and fill in your real WiFi/MQTT credentials
cp src/secrets_example.h src/secrets.h

# Install dependencies via PlatformIO
pio pkg install

# Build & upload the firmware
pio run -t upload
Data Ingestion (Telegraf):

Bash
# Copy the configuration template
cp telegraf_example.conf telegraf.conf

# Add your real InfluxDB Token and MQTT IP to telegraf.conf
# Run the Telegraf agent
telegraf --config telegraf.conf
Dashboard (Node.js):

Bash
cd iot_weather_dashboard

# Install dependencies
npm install

# Create a .env file (InfluxDB token, MQTT broker host, etc.)
# Run the server
node server.js
Note: An MQTT broker (Mosquitto) must be running on the same LAN as the ESP32 and the machine running the dashboard/Telegraf, with its IP address configured across secrets.h, .env, and telegraf.conf.

7. Future Improvements
Add OTA (Over-The-Air) update support so firmware can be updated without a USB cable.

Visualize InfluxDB data through Grafana for more advanced historical charts than the custom dashboard offers.

Add authentication for both the web dashboard and the MQTT broker (currently running with allow_anonymous).

Write unit tests for the firmware modules in the test/ directory.

Deploy the dashboard and Telegraf agent to a server/cloud instance instead of running it locally, to allow remote data viewing.
