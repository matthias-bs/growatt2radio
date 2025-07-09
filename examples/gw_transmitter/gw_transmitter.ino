///////////////////////////////////////////////////////////////////////////////////////////////////
// gw_transmitter.ino
//
// Growatt PV-Inverter Radio Transmitter
// based on SX1276/RFM95W and ESP32
//
// https://github.com/matthias-bs/growatt2radio
//
//
// created: 06/2025
//
//
// MIT License
//
// Copyright (c) 2025 Matthias Prinke
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// History:
//
// 20250628 Created from https://github.com/matthias-bs/SensorTransmitter
// 20250709 Fixed digest position and sleep interval, added log message
//
// ToDo:
// - Change syncword to distinguish messages from bresser protocol
// - Add ID to allow multiple transmitters within range
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <RadioLib.h>
#include <LoraEncoder.h>
#include <growatt_cfg.h>
#include <AppLayer.h>
#include <utils/utils.h>
#include "gw_transmitter.h"

#define SLEEP_INTERVAL 300 // sleep interval in seconds
#define MAX_UPLINK_SIZE 256 // maximum uplink size in bytes

/// Modbus interface select: 0 - USB / 1 - RS485
bool modbusRS485;

/// Application layer
AppLayer appLayer;

// SX1276 has the following connections:
// NSS pin:   PIN_TRANSCEIVER_CS
// DIO0 pin:  PIN_TRANSCEIVER_IRQ
// RESET pin: PIN_TRANSCEIVER_RST
// DIO1 pin:  PIN_TRANSCEIVER_GPIO
static SX1276 radio = new Module(PIN_TRANSCEIVER_CS, PIN_TRANSCEIVER_IRQ, PIN_TRANSCEIVER_RST, PIN_TRANSCEIVER_GPIO);

int msgBegin(uint8_t *msg)
{
    uint8_t preamble[] = {0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t syncword[] = {0x2D, 0xD4};

    memcpy(msg, preamble, sizeof(preamble));
    memcpy(&msg[sizeof(preamble)], syncword, sizeof(syncword));

    return sizeof(preamble) + sizeof(syncword);
}

// setup & execute all device functions ...
void setup()
{
    pinMode(INTERFACE_SEL, INPUT_PULLUP);
    modbusRS485 = digitalRead(INTERFACE_SEL);

    // set baud rate
    if (!modbusRS485)
    {
        Serial.setDebugOutput(false);
        DEBUG_PORT.begin(115200, SERIAL_8N1, DEBUG_RX, DEBUG_TX);
        DEBUG_PORT.setDebugOutput(true);
        log_d("Modbus interface: USB");
    }

    // Initialize Application Layer - starts sensor reception
    appLayer.begin();

    // build payload byte array
    uint8_t uplinkPayload[MAX_UPLINK_SIZE];

    LoraEncoder encoder(uplinkPayload);

#if defined(EMULATE_SENSORS)
    appLayer.genPayload(1 /* fPort */, encoder);
#else
    // get payload immediately before uplink
    appLayer.getPayloadStage2(1 /* fPort */, encoder);
#endif

    uint8_t uplinkSize = encoder.getLength();

    log_i("%s Initializing ... ", TRANSCEIVER_CHIP);
    // carrier frequency:                   868.3 MHz
    // bit rate:                            8.22 kbps
    // frequency deviation:                 57.136417 kHz
    // Rx bandwidth:                        270.0 kHz (CC1101) / 250 kHz (SX1276)
    // output power:                        10 dBm
    // preamble length:                     40 bits
    // Preamble: AA AA AA AA AA
    // Sync: 2D D4
    // SX1276 initialization
    int state = radio.beginFSK(868.3, 8.21, 57.136417, 250, 10, 32);

    if (state == RADIOLIB_ERR_NONE)
    {
        log_i("success!");
    }
    else
    {
        log_e("failed, code %d", state);
        while (true)
            ;
    }

    uint8_t msg_buf[40];
    uint8_t preamble_size;

    preamble_size = msgBegin(msg_buf);

    memcpy(&msg_buf[preamble_size + 2], uplinkPayload, uplinkSize);

    int digest = lfsr_digest16(&msg_buf[preamble_size + 2], uplinkSize, 0x8005, 0xba95);
    digest ^= 0x6df1;

    // | preamble | byte0  | byte1  | byte2 | ... | byteN |
    // | -------- |--------|--------|-------|-----|-------|
    // |          | digest | digest |    <- payload ->    |
    // |          | [15:8] |  [7:0] |                     |
    // |          | <------------- whitening -----------> |

    msg_buf[preamble_size] = digest >> 8;
    msg_buf[preamble_size + 1] = digest & 0xFF;

    for (int i = 0; i < uplinkSize + 2; i++)
    {
        msg_buf[preamble_size + i] ^= 0xAA;
    }

    uint8_t msg_size = preamble_size + 2 + uplinkSize;
    log_i("%s Transmitting packet (%d bytes)... ", TRANSCEIVER_CHIP, msg_size);
    log_message("TX-Data", msg_buf, msg_size);
    state = radio.transmit(msg_buf, msg_size);

    if (state == RADIOLIB_ERR_NONE)
    {
        // the packet was successfully transmitted
        log_i(" success!");

#if defined(USE_SX1276)
        // print measured data rate
        log_i("%s Datarate:\t%f bps", TRANSCEIVER_CHIP, radio.getDataRate());
#endif
    }
    else if (state == RADIOLIB_ERR_PACKET_TOO_LONG)
    {
        // the supplied packet was longer than 256 bytes
        log_e("too long!");
    }
    else if (state == RADIOLIB_ERR_TX_TIMEOUT)
    {
        // timeout occurred while transmitting packet
        log_e("timeout!");
    }
    else
    {
        // some other error occurred
        log_e("failed, code %d", state);
    }

    ESP.deepSleep(SLEEP_INTERVAL * 1000000L);
}

void loop()
{
    // nothing to do here
    // all is done in setup() and the application layer
    // will take care of sensor reception and uplink generation
}
