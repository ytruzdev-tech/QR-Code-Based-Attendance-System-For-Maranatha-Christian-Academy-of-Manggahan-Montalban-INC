# QR-Code-Based-Attendance-System-For-Maranatha-Christian-Academy-of-Manggahan-Montalban-INC
This is a QR code-based attendance system built with the ESP32 microcontroller. Utilizing WiFi, an LCD display, and a QR scanner, it simplifies the process of recording attendance for students. The system also includes a web interface for viewing statistics, exporting logs, and system control.
Features

Wi-Fi Connectivity: Acts as an access point and connects to a router for NTP and remote control.
LCD Interface: Displays system status, date, time, and scan results.
QR Code Scanner: Reads QR codes via serial communication for student and admin commands.
Attendance Recording: Stores student names, sections, date, and time in local FS.
Admin Controls: Login with PIN, view stats, export CSV, reset data.
Automatic Daily Reset: Clears records at midnight.
System Control: Enable/disable via touch sensor.
Remote Commands: Admin functions via QR code commands.
Web Interface: View attendance per grade, stats, download CSV.


Hardware Requirements

ESP32 Dev Board
I2C LCD (e.g., 0x27 address)
Serial QR Scanner (connected to GPIO 16 & 17)
Touch sensor on GPIO 4
Wi-Fi network (AP mode + connection to router)
(Optional) Power supply and peripherals


Setup Instructions

Configure Wi-Fi:

Set your Wi-Fi credentials in the code (ssid, password, router_ssid, router_pass).


Upload Code:

Use Arduino IDE or PlatformIO with ESP32 support.
Ensure LittleFS is installed; format the filesystem during first upload.


Start System:

Power on the device; it initializes Wi-Fi and displays "SYSTEM READY".
Connect to Wi-Fi network ESP32_Attendance_System.
Access via web at the device's IP for stats, reports, and management.


Admin Login:

Double-tap the touch sensor (GPIO 4) to trigger admin login prompts.
Enter PIN: **** for admin access.


QR Commands:

Scan QR codes with specific commands for system control and info.




Usage

Student Attendance:
Scan QR code containing student info: Name Section Grade.
System logs attendance with timestamp.


Admin Controls:
Log in via PIN prompt.
View stats, export CSV, reset records.


Remote Commands:
Admin QR codes for system status, date/time, IP address, etc.


Daily Reset:
Automatically clears records at midnight.




Code Overview

Libraries:

Wire, LiquidCrystal_I2C for LCD
WiFi, WebServer for network/web interface
LittleFS for local file storage
time.h for NTP sync
esp_task_wdt for watchdog timer


Main Functions:

Wi-Fi setup and web server routes for statistics, reports, login.
QR code reading and command handling.
Attendance data management and daily reset.
LCD display updates for status and commands.
Admin authentication via touch sensor input.
