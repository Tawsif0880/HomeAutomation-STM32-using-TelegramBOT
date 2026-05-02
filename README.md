# Home Core: Dual-MCU Adaptive Home Automation and Security System

Project Report

## 1. Introduction

Home Core is a dual-microcontroller smart home automation and security prototype designed to combine local safety control with remote monitoring. The system uses an STM32 microcontroller as the execution layer for time-sensitive sensing, actuator control, interrupt handling, relay switching, OLED feedback, and alarm response. An ESP32 operates as the gateway layer by connecting the hardware system to Wi-Fi, Telegram messaging, and a local web dashboard.

The main objective of the project is to automate household safety, access control, lighting, and ventilation while keeping emergency responses independent from internet latency. The STM32 continuously reads the PIR motion sensor, MQ2 gas sensor, DHT11 temperature-humidity sensor, and fire/flame sensor. It then drives the fan relay, light relay, door lock relay, buzzer, and SSD1306 OLED display according to a priority-based control strategy.

The STM32 and ESP32 communicate through a 9600 baud UART link. The STM32 sends telemetry once per second in the format T, H, M, G, E, L, O, and C, representing temperature, humidity, motion, gas value, emergency state, lockdown state, occupancy mode, and climate mode. The ESP32 parses this telemetry, displays it on a web dashboard, and sends alerts or status reports through Telegram. Commands from Telegram or the web dashboard are forwarded from the ESP32 to the STM32 through the same serial link.

The system follows a safety-first operating order. Emergency conditions such as fire or dangerous gas levels override all other modes by unlocking the door, turning the light on, disabling the fan, and activating the buzzer. Lockdown mode prioritizes security by keeping the door locked, turning off loads, and treating PIR motion as an intrusion event. In normal operation, occupancy-based lighting and temperature-based fan control improve convenience and energy efficiency.


## 2. Problem Statement (What it Solves)

Many basic home automation or home security setups depend on separate devices that do not coordinate with each other. A light switch, a fan controller, a door lock, a gas alarm, and a motion alarm may work individually, but they often lack a common control strategy. This can reduce safety during emergencies and also increases the need for manual operation.

This project addresses that inefficiency by combining environmental sensing, intrusion detection, emergency response, and remote control into one integrated hardware system. Instead of relying only on cloud commands, the STM32 performs local decision-making for emergency events and actuator control. This means the door unlock and buzzer alarm can still operate even if Telegram or Wi-Fi service is delayed or unavailable.

The system also solves practical convenience problems. It can turn the light on when motion is detected, turn the fan on or off based on temperature thresholds, report current home conditions through Telegram, and provide a web dashboard for local monitoring and manual control. As a result, the project provides both safety improvement and automation for a small smart-home model.

## 3. Components and Sensors Used

The hardware stack can be divided into processing units, sensors, output devices, communication/display devices, and supporting construction materials. The component list below is extracted from the STM32 and ESP32 source code and from the project feature list.

- **STM32 microcontroller board: **Acts as the real-time execution controller. It reads sensors through GPIO and ADC, handles a fire-sensor external interrupt, controls active-low relays, updates the OLED display, and exchanges telemetry/commands with the ESP32 over USART1 at 9600 baud.

- **ESP32 development board: **Acts as the network gateway. It connects to Wi-Fi, runs a local WebServer on port 80, polls Telegram Bot API messages, sends Telegram alerts, and forwards web or Telegram commands to the STM32 through Serial2.

- **PIR motion sensor: **Connected to the STM32 as an active-high digital input. It is used for occupancy-based lighting and intrusion detection during lockdown mode. The firmware samples it every 50 ms and confirms the same state three times to reduce false triggering.

- **DHT11 temperature and humidity sensor: **Connected to the STM32 on PB0. The code manually performs the DHT start signal, reads 40 bits, verifies checksum data, and updates temperature and humidity values every 2 seconds.

- **MQ2 gas/smoke sensor: **Connected to STM32 ADC1 channel 0 on PA0. The analog value is sampled every 500 ms. A raw threshold of 2300 is used for gas alarm detection.

- **IR fire/flame sensor: **Connected to STM32 PB1 as an active-low input with falling-edge external interrupt. It is used for immediate fire alarm detection.

- **SSD1306 OLED display: **Driven through STM32 I2C1 at 400 kHz. It displays temperature, humidity, motion state, gas value, and the current operating mode.

- **Relay modules: **Three active-low relay outputs control the fan, light, and electronic door lock. The relays isolate the microcontroller from higher-current loads.

- **Electronic door lock: **Controlled through the door relay. It supports lock, unlock, timed 5-second pulse unlock, and automatic emergency unlock.

- **Fan: **Controlled through the fan relay. It can be switched manually or automatically using temperature hysteresis, with ON at 28 deg C and OFF at 26 deg C.

- **Light: **Controlled through the light relay. It supports manual control and automatic occupancy lighting using PIR motion data.

- **Active buzzer: **Connected to STM32 PB5. It provides a local audible warning for intrusion and emergency states.

- **Power and construction materials: **The prototype uses external power for relay-driven loads, jumper wires, a breadboard or connection board, common ground wiring, and a cardboard house model for physical demonstration. In the prototype, two 9 V batteries were connected in series as an alternative to a 12 V battery source.


## 4. Pin Configuration and Wiring

The following pin configuration is based on the STM32 GPIO, ADC, UART, I2C, and timer initialization shown in the provided code, and on the ESP32 Serial2 configuration. All relay outputs in the STM32 firmware use active-low logic, meaning GPIO reset energizes the relay and GPIO set turns the relay off.


### STM32 Pinout

| Microcontroller Pin | Component Connected | Specific Function |
|---|---|---|
| PA0 / ADC1_IN0 | MQ2 gas/smoke sensor analog output | Analog gas reading using ADC1, software-start conversion, right-aligned data, sampled every 500 ms. Gas alarm threshold is raw value 2300. |
| PA1 | Fan relay input | Active-low digital output. Controls fan relay. Automatic climate mode turns fan ON at 28 deg C and OFF at 26 deg C; manual FANON/FANOFF commands are also supported. |
| PA2 | Light relay input | Active-low digital output. Controls light relay. Used for occupancy lighting and manual LIGHTON/LIGHTOFF commands. |
| PA3 | Door lock relay input | Active-low digital output. Controls electronic door lock relay. Supports lock, unlock, 5-second pulse unlock, and emergency unlock. |
| PA8 | PIR motion sensor output | Digital input with pulldown. Active-high motion detection. Firmware samples every 50 ms and uses three confirmations before accepting a stable state. |
| PB0 | DHT11 temperature-humidity data pin | Single-wire DHT data line. The firmware reconfigures this pin between output and input modes and uses TIM2 for microsecond timing. |
| PB1 / EXTI1 | IR fire/flame sensor output | Digital input with pull-up and falling-edge interrupt. Active-low fire detection triggers emergency logic. |
| PB5 | Active buzzer | Digital output. Provides toggled intrusion alarm and continuous emergency alarm. |
| I2C1 SCL/SDA | SSD1306 OLED display | I2C display interface configured at 400 kHz. Exact SCL/SDA GPIO pins are not shown in the provided main.c GPIO excerpt and should be verified from the CubeMX/Msp configuration. |
| USART1 TX/RX | ESP32 Serial2 UART link | Bidirectional 9600 baud, 8N1 communication between STM32 and ESP32. Exact STM32 UART GPIO pins are configured by HAL MSP code and are not shown in the provided main.c excerpt. |
| TIM2 | DHT11 timing support | Base timer with prescaler 7 and period 65535, used for microsecond-level DHT11 pulse timing. |
| TIM4 Channel 4 | Reserved or unused PWM output | PWM configuration exists in the source, but MX_TIM4_Init is not called in the shown main initialization sequence and no current actuator logic uses TIM4. |

### ESP32 Pinout

| Microcontroller Pin / Interface | Component Connected | Specific Function |
|---|---|---|
| GPIO16 / Serial2 RX | STM32 USART1 TX | Receives telemetry, acknowledgements, alarm lines, motion events, and the emergency byte 0xFF from the STM32. Defined in code as STM32_RX_PIN. |
| GPIO17 / Serial2 TX | STM32 USART1 RX | Transmits commands such as LOCK, UNLOCK, FANON, FANOFF, LIGHTON, LIGHTOFF, DOORUNLOCK, LOCKDOWN_ON/OFF, OCCUPANCY_ON/OFF, CLIMATE_ON/OFF, and CLEARALARM to the STM32. Defined in code as STM32_TX_PIN. |
| USB Serial interface | Computer serial monitor | Debug logging at 115200 baud, including Wi-Fi connection status, web request logs, Telegram request logs, and STM32 acknowledgements. |
| Wi-Fi radio | Router, local browser clients, Telegram Bot API | Provides network connection for the local HOME CORE web dashboard, HTTP routes, Telegram bot polling, remote status display, and alert messages. |

Communication summary: the STM32 sends telemetry as text lines such as T=temperature,H=humidity,M=motion,G=gas,E=emergency,L=lockdown,O=occupancy,C=climate. It also sends PIR=MOTION, ALARM=FIRE, ALARM=GAS, ALARM=INTRUSION, LOCKED, UNLOCKED, and a raw emergency byte 0xFF. The ESP32 web dashboard and Telegram handler convert user actions into UART commands and forward them to the STM32. The updated ESP32 firmware also hosts a local HOME CORE web page that auto-refreshes and shows temperature, humidity, motion, gas value, mode, alarm state, and door state.

## 5. Cost Analysis

The following table is intentionally left blank so that component prices can be filled manually according to the actual purchase source and local market price.

| Component | Quantity | Unit Cost | Total Cost |
|---|---|---|---|
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |
|  |  |  |  |

## 6. Difficulties Faced

Several technical and practical challenges were encountered during the development of the prototype. These difficulties affected sensor learning, power selection, model construction, component reliability, and microcontroller board selection.

- **Learning new sensor behavior: **Different sensors were used in the same project, and many of them were new to the team. Before implementation, it was necessary to understand how each sensor worked, including PIR motion output behavior, DHT11 timing and checksum requirements, MQ2 analog gas readings, and the active-low response of the fire/flame sensor.

- **Powering relay-controlled loads: **The fan, light, and door lock required a higher-voltage external power source through relay modules. A 12 V battery was needed, but it was expensive for the prototype. As an alternative, two 9 V batteries were connected in series to produce approximately 18 V. This solved the availability issue but made power management and wiring more delicate.

- **Cardboard house construction: **Building the physical house model with cardboard was difficult because the team was not used to mechanical model-making. The cardboard structure had to support sensors, wiring, relays, the door-lock mechanism, and the visual presentation of the smart-home system.

- **Electrical component uncertainty: **Because the project involved many electrical modules, progress depended heavily on whether each component was functioning correctly. Sensor modules, relays, wires, and the development boards had to be checked repeatedly during testing.

- **Faulty or fake STM32 board issue: **The original STM32 board did not provide the expected 5 V output and only supplied 3.3 V. With help from the lab instructor, the issue was diagnosed and the board was replaced, allowing the project to continue with a suitable STM32 board.

## 7. Limitations

The current implementation is functional for a prototype demonstration, but it has several limitations that should be addressed before using a similar system in a real home environment.

- **Power-supply limitation: **Using two 9 V batteries in series is a temporary prototype solution and may not provide stable current for relay-driven loads such as a fan or electronic lock. A regulated 12 V supply or rechargeable battery pack with proper current rating, voltage regulation, fuse protection, and flyback protection would be more reliable.

- **Sensor calibration limitation: **The MQ2 gas alarm uses a fixed raw ADC threshold of 2300. Gas sensors are affected by warm-up time, sensor age, airflow, and environmental conditions. A future version should include calibration, baseline tracking, and adjustable threshold settings.

- **Environmental sensing limitation: **The DHT11 sensor is low-cost but has limited accuracy and a slow response rate. The firmware samples it every 2 seconds, which is suitable for demonstration but not ideal for precise environmental control. A DHT22, SHT31, or BME280 could improve measurement quality.

- **Network dependency for remote features: **Local emergency actions run on the STM32, but Telegram alerts, Telegram commands, and the web dashboard depend on Wi-Fi and internet availability. A future version could add offline event storage, GSM backup, or a local authenticated mobile app.

- **Security limitation: **The ESP32 code contains Wi-Fi, bot-token, and passphrase configuration fields directly in source code, uses setInsecure for the secure client, and exposes a local web dashboard without authentication. It also stores authorization as a global state rather than strictly validating every incoming Telegram command against the stored authorized chat ID. For a production design, secrets should be stored securely, certificates should be validated, the bot token should be rotated if exposed, the dashboard should require authentication, and every command should be checked against the authorized chat ID.

- **Communication robustness limitation: **The STM32 and ESP32 exchange plain UART text commands and telemetry without checksum, sequence number, retry logic, or encryption. For improved reliability, a framed protocol with CRC, acknowledgements, timeout handling, and command validation should be implemented.

- **Data logging limitation: **The ESP32 stores only the latest temperature, humidity, gas, motion, mode, alarm, and door values in RAM. The system does not maintain long-term logs or trends. A future version could store readings in flash memory, an SD card, or a cloud database.

- **Documentation and wiring limitation: **The provided STM32 main.c clearly shows GPIO assignments for sensors, relays, buzzer, and ADC input. However, the exact GPIO mappings for I2C1 and USART1 are not shown in the provided main.c excerpt because they are normally configured in MSP or CubeMX-generated files. A complete schematic should be prepared for final reproducibility.

## 8. Conclusion

The Home Core project successfully demonstrates a dual-microcontroller approach to smart-home automation and security. By assigning real-time hardware control to the STM32 and network communication to the ESP32, the system separates safety-critical tasks from internet-dependent tasks. This makes the design more responsive and more reliable than a system that depends entirely on cloud communication.

The STM32 firmware integrates motion sensing, gas detection, fire detection, temperature and humidity measurement, relay control, buzzer alarm logic, OLED display output, and serial telemetry. The updated ESP32 firmware expands the system by adding a local HOME CORE web dashboard, Telegram-based authentication, Telegram status reporting, remote manual commands, and automatic alerts for motion, intrusion, gas, and fire events.

Overall, the project achieved its objective of combining security, automation, access control, and remote monitoring in one prototype. With improvements in power regulation, sensor calibration, secure communication, command authentication, and event logging, the system can be developed into a more robust smart-home safety platform.