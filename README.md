Project Report
CSE331L: Microprocessor Interfacing & Embedded Systems Lab
Faculty: Kazi Safkat Taa Seen (KSE)
Lab Instructor: Jannat Sultana
Section: 04, Fall 2025
Project Title: Smart Greenhouse Cultivation, Irrigation & Safety Monitoring System
Smart Greenhouse Cultivation, Irrigation & Safety Monitoring System
Team Members:
Name	Student ID
Md. Nasim Ahmed	2232936042
Md. Asraful Hossain Riyan	2232845642
Hasinur Rahman Rohan	1912835642
Hassan Abdirahman Abdullahi	2212938045

 
1.	 Project Title
Smart Greenhouse Cultivation, Irrigation & Safety Monitoring System.
2.	 Abstract 
Greenhouse cultivation significantly improves crop yield by maintaining ideal environmental conditions. However, manually monitoring temperature, humidity, and soil moisture is labor-intensive and prone to human error. This project addresses these challenges by implementing a low-cost, automated Smart Greenhouse system using the STM32F103C8T6 (Blue Pill) microcontroller. The system performs real-time environmental sensing, automated irrigation control based on soil conditions, and safety monitoring.
The prototype utilizes a DHT11 sensor for temperature and humidity, an analog soil moisture sensor to trigger automatic irrigation via a servo motor, and a flame sensor to detect fire hazards immediately. Additionally, an ultrasonic sensor is employed to detect motion near the greenhouse entrance for security. A 0.96-inch OLED display provides a local user interface, while a 4x4 keypad allows for manual control and menu navigation. Testing confirmed that the system successfully automates irrigation when soil is dry and triggers instant audio-visual alarms during safety events, demonstrating a practical application of embedded systems in smart agriculture.

3.	 Introduction and Motivation
Introduction: Greenhouses regulate climate for optimal crop growth, but manual management is inefficient. This project introduces a "Smart Greenhouse" prototype that uses electronic sensors to continuously monitor environmental conditions and safety hazards, bridging the gap between traditional farming and modern automation.
Motivation:
•	Real-World Problem: Small-scale farmers often rely on guesswork for soil moisture and temperature, leading to crop stress. Additionally, fire hazards or intrusions may go unnoticed without automated alerts.
•	Relevance: This system offers an affordable entry into "Smart Agriculture," reducing physical labor while providing immediate visual feedback and safety warnings.
•	Course Relation: 
GPIO & Interrupts: Used for the Keypad scanning and Flame sensor (EXTI) for immediate response.
ADC (Analog-to-Digital Conversion): Used to interpret analog signals from the Soil Moisture sensor. 
Timers & PWM: Essential for controlling the Servo motor (valve) and reading the Ultrasonic sensor.
I2C Protocol: Implemented for efficient communication with the OLED display.


4.	 System Overview
The system is built around the STM32 Blue Pill microcontroller, which acts as the central brain. It continuously polls data from various sensors to monitor the greenhouse environment.

The overall architecture and signal flow are summarized in Figure 1.
 
Figure 1: System block diagram.

Feature talk:

• Environmental monitoring - Displays real-time temperature/humidity (DHT11) and soil moisture level (ADC) on the OLED.
• Automatic irrigation decision - When soil moisture falls below the threshold, the servo opens the valve; it closes when moisture returns to normal.
• Fire/overheating alert - Flame sensor input is handled via interrupt to ensure fast response; buzzer and OLED warning activate immediately.
• Intrusion/motion detection - Ultrasonic sensing detects nearby movement; the system can switch to security mode and raise alerts.
• On-device user interface - Keypad-driven menu enables viewing pages, manual overrides, and configuration without a PC.

5.	 Implementation Details
a.	Hardware Implementation


	Table 1 lists the final hardware modules used in the prototype along with justification 	and an estimated cost breakdown (prices may vary by vendor).

Table 1: Final hardware module list and estimated cost breakdown.
 Hardware Module	Purpose	Interface	Qty	Est. Unit Cost (BDT)	Justification
STM32F103C8T6 (Blue Pill)	Main controller for sensor interfacing and control logic	GPIO/ADC/Timers/I2C	1	350	Low-cost ARM Cortex-M3 MCU with rich peripherals
ST-LINK V2	Programming and debugging interface	SWD	1	400	Reliable flashing and debugging for STM32
DHT11	Temperature and humidity measurement	Single-wire GPIO	1	80	Simple and commonly available sensor
Soil Moisture Sensor (analog)	Soil condition monitoring for irrigation	ADC	1	120	Analog output allows threshold-based control
Flame/Heat Sensor	Fire/overheating detection	Digital GPIO (EXTI)	1	70	Fast event detection using interrupt
Ultrasonic Sensor (HC-SR04)	Motion/distance sensing	Timer/GPIO	1	130	Inexpensive distance sensor for security feature
0.96" OLED Display (SSD1306)	User display for values, menus, and alerts	I2C	1	350	Low power, high contrast; simple I2C wiring
4x4 Matrix Keypad	User input for menu and control	GPIO matrix scan	1	120	Simple UI input device without extra components
Servo Motor (SG90/MG90S)	Valve/door actuation	PWM	1	250	Direct position control suited for valve mechanism
Buzzer	Audible alarm output	GPIO	1	20	Immediate alert with minimal circuitry
Breadboard + Jumper Wires	Prototype interconnections	N/A	1 set	220	Rapid prototyping and testing
Misc. components	Signal conditioning and safe driving	N/A	1 set	80	Improves reliability (resistors, transistor, connectors)
Power supply / 5V source	Stable power for sensors and actuators	N/A	1	200	Required for stable sensor readings and servo operation
Estimated Total				2390	


The prototype was assembled on a breadboard with the Blue Pill as the central 		board. Sensors were placed on the periphery to simplify wiring, and the OLED was 	mounted on the top side for easy viewing. The servo was powered from a stable 5V 	source (with common ground to the STM32) to avoid brown-outs during movement. 
	Figure 2 illustrates a high-level wiring/interface mapping used in the prototype.


 
Figure 2: High-level wiring/interface diagram (prototype).

b.	Software Implementation
The firmware was implemented in C using the STM32 HAL libraries (STM32CubeMX/CubeIDE or Keil uVision with ARMCLANG) and programmed via ST-LINK. 
A modular driver approach was followed: separate functions/modules handle OLED (SSD1306 over I2C), DHT11 timing, keypad scanning, ultrasonic measurement, servo PWM control, and ADC sampling for soil moisture.

Control is organized as a simple state machine with periodic sensing and event-driven interruptions. In each loop iteration, the system scans the keypad to update the active page/mode, reads sensors, evaluates thresholds, and updates actuators. Safety events (flame detection) are handled with higher priority to ensure fast alarm activation. Figure 3 shows the high-level control flow.

 
Figure 3: High-level control flow.

Interfaces and protocols used in the prototype:
• I2C: OLED display communication (SSD1306).
• ADC: Soil moisture sensor analog sampling (with optional averaging/filtering).
• GPIO matrix scanning: Keypad row/column scanning with pull-ups and debouncing.
• Timers / PWM: Servo control (PWM) and ultrasonic echo timing (input capture or microsecond timing).
• EXTI interrupt: Flame sensor alert for immediate response.
6.	 Discussion
•	We successfully integrated multiple sensors into a cohesive dashboard. The I2C OLED provides clear data, and the safety interrupts (flame detection) react instantly. The servo motor integration proves the system's ability to physically actuate based on commands.
•	Expected vs. actual outcomes:
All major features from the proposal (environment monitoring, flame alert, motion detection, and local UI) were implemented at prototype level.
•	Key challenges and mitigation:
DHT11 timing sensitivity required careful microsecond delays and avoidance of blocking operations during sampling. Keypad scanning needed debouncing to prevent multiple detections per press. Servo actuation introduced supply noise, so a separate 5V supply (with common ground) and basic filtering were used to keep readings stable.
•	Deviations from the original plan:
The proposal included water tank management and an automatic irrigation system. We were told not to implement them; thus, they were not bought or implemented. They had a prototype in the project as an option for water filling was represented as an alert/indicator which was modified accordingly.
•	Limitations and future improvements:
The prototype does not include remote monitoring (Wi-Fi/GSM) or long-term data logging. Sensor accuracy is limited by DHT11 and low-cost analog probes, and the system requires calibration per soil type. 
Future work includes adding ESP8266/ESP32 connectivity, SD card logging, using higher-accuracy sensors (DHT22/SHT3x), implementing closed-loop control for irrigation, and designing a PCB for robust deployment.




7.	 Appendix (optional)
Appendix A provides an example pin map used during prototyping. Pin assignments may be updated depending on the CubeMX configuration.

Module	Signal(s)	Example STM32 Pin(s)
Soil Moisture	Analog out	PA0 (ADC1_IN0)
DHT11	DATA	PA1
Flame Sensor	Digital out	PA2 (EXTI)
Ultrasonic	TRIG / ECHO	PA3 / PB1
OLED (I2C)	SCL / SDA	PB6 / PB7
Servo	PWM	PA8 (TIM1_CH1)
Buzzer	Digital	PB0
Keypad	Rows / Columns	PB8–PB11 / PA4–PA7
Table 2: Example pin mapping used in the prototype.

