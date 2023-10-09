# Hydrogarden


Hydrogarden is a project of mine, aiming to automatise watering of my home garden. This repo contains code for ESP32
microcontroller which allows for remote control of each of the 8 circuits.
You can toggle the state of circuits, set their state, get the state of a specific circuit, get the state of all circuits, and more.

If you are interested, you can find REST API and frontend client for Hydrogarden in separate repositories.

## Table of Contents

- [Hydrogarden](#hydrogarden)
  - [Table of Contents](#table-of-contents)
  - [Prerequisites](#prerequisites)
  - [Setup](#setup)
  - [Usage](#usage)
  - [Commands](#commands)
  - [License](#license)

## Prerequisites

Before using this code, you'll need the following:

- Arduino IDE installed on your computer.
- An Arduino board with Wi-Fi capabilities (e.g., ESP32).
- A stable Wi-Fi connection.
- The required libraries installed: `EEPROM`, `WiFi`, `HTTPClient`, `ArduinoJson`.

## Setup

1. Upload the code to your Arduino board using the Arduino IDE.
2. Ensure your Arduino board is connected to the appropriate pins or relays that control the circuits you want to manage. If you are using ESP32 devkit, I recomend powering it with VIN and GND pin, as it allows for a stronger WIFI signal. 
3. Set up your Wi-Fi connection by editing the `WIFI_SSID`, `WIFI_PASSWORD` and `SERVER_URL` constants in the `secrets.h` file in the root file.
4. Redefine pinFromCircuitCodeFunction mapping to match physical connections of your esp32.
   
## Usage

Once the code is uploaded and your Arduino board is connected to your circuits and Wi-Fi, you can control the hydroponic system using both HTTP requests and serial input.

Simply, connect the device pins to apropriate valve drivers according to pinFromCircuitCode mapping.

## Commands

You can send commands to control the circuits via serial input. Here are the available commands:

- `toggle <circuitCode>`: Toggle the state of a specific circuit.
- `set <circuitCode> <mode>`: Set the state of a specific circuit (0 for off, 1 for on).
- `get <circuitCode>`: Get the state of a specific circuit.
- `getall`: Get the state of all circuits.
- `help`: Display the list of available commands.

Example serial command usage:
```
toggle 1
set 2 1
get 3
getall
help
```

You can also send HTTP requests to control the circuits. For example, you can send a POST request to `http://your-server-ip/api/hydroponic/confirm-execution-of-task` to confirm the execution of a task. You can retrieve tasks to perform by sending a GET request to `http://your-server-ip/api/hydroponic/get-task`.

## License

This code is released under the [MIT License](LICENSE). Feel free to modify and distribute it as needed for your hydroponic control project.