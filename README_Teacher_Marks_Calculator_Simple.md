# Teacher Marks Calculator

Hi! This is my ESP32-based Teacher Marks Calculator. I made it to enter students' marks, calculate results and view student rankings.

## Features
- Enter marks for 6 subjects.
- Calculate total and percentage.
- Check pass or fail.
- View student rankings.
- Edit student and subject names using Wi-Fi.
- Download marks as a CSV file.

## Components Needed
- ESP32 development board
- TFT display
- 4x4 keypad
- Jumper wires
- Breadboard
- USB data cable

## ESP32 Keypad Connections

| Keypad Pin | ESP32 GPIO |
|---|---:|
| R1 | 13 |
| R2 | 14 |
| R3 | 27 |
| R4 | 26 |
| C1 | 25 |
| C2 | 33 |
| C3 | 32 |
| C4 | 16 |

**TFT display:** I use the TFT_eSPI library. The TFT pin connections depend on the display model, so I need to set the correct display driver and pins in the library's setup file.

## Required Libraries
Install these libraries in Arduino IDE:
- TFT_eSPI
- Keypad

ESP32 Wi-Fi and storage libraries are included with the ESP32 board package.

## How to Upload the Code
1. Install Arduino IDE and ESP32 board support.
2. Install the libraries listed above.
3. Configure TFT_eSPI for my TFT display.
4. Open `Teacher_Marks_Calculator_SD_Fixed.ino`.
5. Connect the ESP32 to the computer using a USB data cable.
6. Select my ESP32 board and COM port.
7. Click **Verify**, then **Upload**.
8. If uploading gets stuck at `Connecting...`, hold the BOOT button while the upload starts.

## Wi-Fi Setup
1. Long-press `#` on the keypad to start Wi-Fi mode.
2. Connect to **Marks_Name_Edit**.
3. Password: **12345678**
4. Open the IP address shown on the TFT screen in a browser.
5. Edit names or download the marks CSV file.
6. Press `*` to exit Wi-Fi mode.

## Project Settings
- 6 subjects
- 30 students by default
- Up to 50 students
- Pass mark: 35

This is my project. I hope it is useful, and I welcome suggestions and improvements!
