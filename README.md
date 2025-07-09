# growatt2radio

[growatt2lorawan-v2](https://github.com/matthias-bs/growatt2lorawan-v2) worked totally fine for quite a while, but now the LoRaWAN node cannot communicate with any public gateway (The Things Network or Helium Network) any more from its present location. Presumably a new building and/or its scaffolding within line-of-sight between node and TTN gateway are blocking the radio signal path.

This project is a workaround based on point-to-point radio transmission at 868 MHz using FSK modulation.

The implementation is based on a few existing projects.

## Hardware

ESP32 with SX1276 radio module &ndash;

![firebeetle_esp32+cover_lora](https://user-images.githubusercontent.com/83612361/233463592-e99a9d1c-5100-4ac2-9b33-bcfc974406f0.jpg)
* [DFR0478](https://www.dfrobot.com/product-1590.html) - FireBeetle ESP32 IoT Microcontroller
* [TEL0125](https://www.dfrobot.com/product-1831.html) - LoRa Radio 868MHz - FireBeetle Covers
* 868MHz antenna

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
