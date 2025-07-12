# growatt2radio

[![CI](https://github.com/matthias-bs/growatt2radio/actions/workflows/CI.yml/badge.svg)](https://github.com/matthias-bs/growatt2radio/actions/workflows/CI.yml)
[![GitHub release](https://img.shields.io/github/release/matthias-bs/growatt2radio?maxAge=3600)](https://github.com/matthias-bs/growatt2radio/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](https://github.com/matthias-bs/growatt2radio/blob/main/LICENSE)

[growatt2lorawan-v2](https://github.com/matthias-bs/growatt2lorawan-v2) worked totally fine for quite a while, but now my LoRaWAN node cannot communicate with any public gateway (The Things Network or Helium Network) any more from its present location. Presumably a new building and/or its scaffolding within line-of-sight between node and TTN gateway are blocking the radio signal path.

This project is a workaround based on point-to-point radio transmission at 868 MHz using FSK modulation.

The implementation is based on [a few existing projects](#concept-draft).

## Contents

* [Hardware Requirements](#hardware-requirements)
  * [Hardware Example (Transmitter)](#hardware-example-transmitter)
* [Inverter Modbus Interface Options](#inverter-modbus-interface-options)
  * [Modbus Interface Select Input](#modbus-interface-select-input)
* [Power Supply](#power-supply)
* [Pinning Configuration](#pinning-configuration)
  * [Modbus Interface Selection](#modbus-interface-selection)
  * [Modbus via RS485 Interface](#modbus-via-rs485-interface)
  * [Debug Interface in case of using Modbus via USB Interface (optional)](#debug-interface-in-case-of-using-modbus-via-usb-interface-optional)
* [Library Dependencies](#library-dependencies)
* [Software Build Configuration](#software-build-configuration)
* MQTT Integration
  * [IoT MQTT Panel Example](#iot-mqtt-panel-example)
  * [Datacake Integration](#datacake-integration)
  * [Home Assistant Integration](#home-assistant-integration)
* [Legal](#legal)

## Hardware Requirements

* ESP32 (optionally with LiPo battery charger and battery)
* SX1276 or SX1262 (or compatible) LoRaWAN Radio Transceiver
* 868 MHz Antenna
* Transmitter
  * optional: RS485 Transceiver - 3.3V compatible, half-duplex capable (e.g [Waveshare 4777](https://www.waveshare.com/wiki/RS485_Board_(3.3V)) module)
  * optional: USB-to-TTL converter for Debugging (e.g. [AZ Delivery HW-598](https://www.az-delivery.de/en/products/hw-598-usb-auf-seriell-adapter-mit-cp2102-chip-und-kabel))

### Hardware Example (Transmitter)

![firebeetle_esp32+cover_lora](https://user-images.githubusercontent.com/83612361/233463592-e99a9d1c-5100-4ac2-9b33-bcfc974406f0.jpg)
* [DFR0478](https://www.dfrobot.com/product-1590.html) - FireBeetle ESP32 IoT Microcontroller
* [TEL0125](https://www.dfrobot.com/product-1831.html) - LoRa Radio 868MHz - FireBeetle Covers
* 868 MHz Antenna

## Inverter Modbus Interface Options

1. USB Interface

    The inverter's USB port operates like a USB serial port (UART) interface at 115200 bits/s. If the length of a standard USB cable is sufficient to connect the ESP32 to the inverter (and there are no compatibility issues with the ESP32 board's USB serial interface), this is the easiest variant, because no extra hardware is needed.
    
    As pointed out in [otti/Growatt_ShineWiFi-S](https://github.com/otti/Growatt_ShineWiFi-S/blob/master/README.md), **only CH340-based USB-Serial converters are compatible** - converters with CP21XX and FTDI chips **do not work**!

2. COM Interface

    The inverter's COM port provides an RS485 interface at 9600 bits/s. An RS485 tranceiver is required to connect it to the ESP32.

### Modbus Interface Select Input

The desired interface is selected by pulling the GPIO pin `INTERFACE_SEL` (defined in `settings.h`) to 3.3V or GND, respectively:

| Level | Modbus Interface Selection |
| ----- | ---------------------------- |
| low (GND) | USB Interface |
| high (3.3V/open) | RS485 Interface |

## Power Supply

The ESP32 development board can be powered from the inverter's USB port **which only provides power if the inverter is active**.

**No sun - no power - no transmission!** :sunglasses:

But: Some ESP32 boards have an integrated LiPo battery charger. You could power the board from a battery while there is no PV power (at least for a few hours). 

## Pinning Configuration

See [src/growatt_cfg.h](src/growatt_cfg.h)

### Modbus Interface Selection

| GPIO define | Description |
| ----------- | ----------- |
| INTERFACE_SEL | Modbus Interface Selection (USB/RS485) |

### Modbus via RS485 Interface

| GPIO define   | Waveshare 4777 pin  |
| ------------- | ------------------- |
| MAX485_DE     | RSE                 |
| MAX485_RE_NEG | n.c.                |
| MAX485_RX     | RO                  |
| MAX485_TX     | DI                  |

### Debug Interface in case of using Modbus via USB Interface (optional)

USB-to-TTL converter, e.g. [AZ Delivery HW-598](https://www.az-delivery.de/en/products/hw-598-usb-auf-seriell-adapter-mit-cp2102-chip-und-kabel)

| GPIO define | USB to TTL Converter |
| ----------- | -------------------- | 
| DEBUG_TX    | RXD                  |
| DEBUG_RX    | TXD / n.c.           |

## Library Dependencies

* [ModbusMaster](https://github.com/4-20ma/ModbusMaster) by Doc Walker
* [RadioLib](https://github.com/jgromes/RadioLib) by Jan Gromeš
* [arduino-mqtt](https://github.com/256dpi/arduino-mqtt) by Joël Gähwiler
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson) by Benoît Blanchon
* [Lora-Serialization](https://github.com/thesolarnomad/lora-serialization) by Joscha Feth

## Software Build Configuration

* Install the Arduino ESP32 board package in the Arduino IDE
* Install all libraries listed in [package.json](package.json) &mdash; section `dependencies` &mdash; via the Arduino IDE Library Manager 
* Clone (or download and unpack) the latest ([growatt2radio release](https://github.com/matthias-bs/growatt2radio/releases))
* Transmitter
  * Select your ESP32 board
  * Check / modify pin configuration in [src/growatt_cfg.h](src/growatt_cfg.h)
  * Build and upload [examples/gw_transmitter/gw_transmitter.ino](examples/gw_transmitter/gw_transmitter.ino)
* Receiver
  * Select your ESP32 board
  * Set your WiFi and MQTT credentials in `examples/gw_receiver/secrets.h`
  * Build and upload [examples/gw_receiver/gw_receiver.ino](examples/gw_receiver/gw_receiver.ino)

## MQTT Integration

### IoT MQTT Panel Example

Arduino App: [IoT MQTT Panel](https://snrlab.in/iot/iot-mqtt-panel-user-guide)

<img src="https://user-images.githubusercontent.com/83612361/225129950-c323e0c7-a58b-4a3f-ba30-e0fd9adc1594.jpg" alt="Screenshot_20230314-130325_IoT_MQTT_Panel_Pro-1" width="400">

You can either edit the provided [IoT MQTT Panel configuration file](scripts/iot_mqtt_panel_growatt2radio.json) before importing it or import it as-is and make the required changes in *IoT MQTT Panel*. Don't forget to add the broker's certificate if using secure MQTT! (in the App: *Connections -> Edit Connections: Certificate path*.)

**Editing [scripts/iot_mqtt_panel_growatt2radio.json](scripts/iot_mqtt_panel_growatt2radio.json)**

Change `CONNECTIONNAME`, `YOURMQTTBROKER`, `USERNAME` and `PASSWORD` as needed:
```
[...]
"connectionName": "CONNECTIONNAME",
"host": "YOURMQTTBROKER",
[...]
"username":"USERNAME","password":"PASSWORD"
[...]
```

### Datacake Integration

See [Datacake MQTT Integration Documentation](https://docs.datacake.de/integrations/mqtt).

[scripts/datacake_uplink_decoder.js](scripts/datacake_uplink_decoder.js) is an example [payload decoder](https://docs.datacake.de/integrations/mqtt#write-payload-decoder). Change `device_id` and the MQTT topic variables as required.

With Datacake, you can get [data reports](https://docs.datacake.de/best-practices/best-practices-reports) as CSV files at regular intervals.

### Home Assistant Integration

See [scripts/home_assistant_configuration.yaml](scripts/home_assistant_configuration.yaml)

![home_assistant](https://github.com/user-attachments/assets/491dbb38-bb3e-4b93-a675-d21f1bf9f9cd)

## Concept draft

### Transmitter

* https://github.com/matthias-bs/growatt2lorawan-v2/blob/main/src/growattInterface.h
* https://github.com/matthias-bs/growatt2lorawan-v2/blob/main/src/growattInterface.cpp
* https://github.com/matthias-bs/growatt2lorawan-v2/blob/main/src/AppLayer.h
* https://github.com/matthias-bs/growatt2lorawan-v2/blob/main/src/AppLayer.cpp
* Arduino sketch
   * FSK transmitter configuration based on https://github.com/matthias-bs/SensorTransmitter/blob/main/SensorTransmitter.ino (uses [RadioLib](https://github.com/jgromes/RadioLib))
   * Operating sequence
     1. Power-on / wake-up 
     2. Read data from PV inverter via Modbus interface
     3. Pack data into binary buffer
     4. Whitening / encryption?
     5. Add preamble, digest and transmitter ID
     6. Transmit packet via radio modem
     7. Sleep

### Receiver
* https://github.com/matthias-bs/BresserWeatherSensorReceiver/tree/main/src
* https://github.com/matthias-bs/BresserWeatherSensorReceiver/blob/main/examples/BresserWeatherSensorMQTT/BresserWeatherSensorMQTT.ino
* Arduino sketch
  * FSK receiver configuration and based on [BresserWeatherSensorReceiver](https://github.com/matthias-bs/BresserWeatherSensorReceiver) (uses [RadioLib](https://github.com/jgromes/RadioLib))
  * Operating sequence
    1. Power-on / wake-up
    2. Wait for radio message preamble
    3. Receive message
    4. Validate digest and transmitter ID (optional)
    5. De-whitening / decryption?
    6. Convert binary payload buffer to JSON (reverse [lora-serialization](https://github.com/thesolarnomad/lora-serialization))
    7. Publish JSON data using MQTT via WiFi
    8. Sleep

## Legal

> This project is in no way affiliated with, authorized, maintained, sponsored or endorsed by Growatt or any of its affiliates or subsidiaries.
