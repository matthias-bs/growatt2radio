///////////////////////////////////////////////////////////////////////////////////////////////////
// gw_receiver.ino
//
// Growatt PV-Inverter Radio Receiver
// based on SX1276/RFM95W and ESP32
//
// https://github.com/matthias-bs/growatt2radio
//
// Growatt PV-Inverter Radio Receiver
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
// 20250628 Created from https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// ToDo: 
// - 
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <RadioLib.h>
#include <LoraEncoder.h>
#include <ArduinoJson.h>
#include <growatt_cfg.h>
#include <utils/utils.h>
#include "gw_receiver.h"

#define SLEEP_INTERVAL 300 // sleep interval in seconds
#define RX_TIMEOUT 10000   // sensor receive timeout [ms]
#define MSG_BUF_SIZE 30    // last byte of preamble + digest (2 bytes) + payload (27 bytes)

static int rssi = 0; // variable to hold the RSSI value

// Flag to indicate that a packet was received
volatile bool receivedFlag = false;

// This function is called when a complete packet is received by the module
// IMPORTANT: This function MUST be 'void' type and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
    IRAM_ATTR
#endif
void setFlag(void)
{
    // We got a packet, set the flag
    receivedFlag = true;
}

// SX1276 has the following connections:
// NSS pin:   PIN_TRANSCEIVER_CS
// DIO0 pin:  PIN_TRANSCEIVER_IRQ
// RESET pin: PIN_TRANSCEIVER_RST
// DIO1 pin:  PIN_TRANSCEIVER_GPIO
static SX1276 radio = new Module(PIN_TRANSCEIVER_CS, PIN_TRANSCEIVER_IRQ, PIN_TRANSCEIVER_RST, PIN_TRANSCEIVER_GPIO);

struct modbus_input_registers
{
  int status;
  float solarpower, pv1voltage, pv1current, pv1power, pv2voltage, pv2current, pv2power, outputpower, gridfrequency, gridvoltage;
  float energytoday, energytotal, totalworktime, pv1energytoday, pv1energytotal, pv2energytoday, pv2energytotal, opfullpower;
  float tempinverter, tempipm, tempboost;
  int ipf, realoppercent, deratingmode, faultcode, faultbitcode, warningbitcode;
};
struct modbus_input_registers modbusdata;

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
/*!
 * \brief Log message payload
 *
 * \param descr    Description.
 * \param msg      Message buffer.
 * \param msgSize  Message size.
 *
 * Result (example):
 *  Byte #: 00 01 02 03...
 * <descr>: DE AD BE EF...
 */
void log_message(const char *descr, const uint8_t *msg, uint8_t msgSize) {
    char buf[128];
    const char txt[] = "Byte #: ";
    int offs;
    int len1 = strlen(txt);
    int len2 = strlen(descr) + 2; // add colon and space
    int prefix_len = max(len1, len2);

    memset(buf, ' ', prefix_len);
    buf[prefix_len] = '\0';
    offs = (len1 < len2) ? (len2 - len1) : 0;
    strcpy(&buf[offs], txt);
  
    // Print byte index
    for (size_t i = 0 ; i < msgSize; i++) {
        sprintf(&buf[strlen(buf)], "%02d ", i);
    }
    log_d("%s", buf);

    memset(buf, ' ', prefix_len);
    buf[prefix_len] ='\0';
    offs = (len1 > len2) ? (len1 - len2) : 0;
    sprintf(&buf[offs], "%s: ", descr);
  
    for (size_t i = 0 ; i < msgSize; i++) {
        sprintf(&buf[strlen(buf)], "%02X ", msg[i]);
    }
    log_d("%s", buf);
}
#endif

int16_t setupRadio()
{
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

    if (state == RADIOLIB_ERR_NONE)
    {
        log_d("success!");
        state = radio.fixedPacketLengthMode(MSG_BUF_SIZE);
        if (state != RADIOLIB_ERR_NONE)
        {
            log_e("%s Error setting fixed packet length: [%d]", TRANSCEIVER_CHIP, state);
            while (true)
                delay(10);
        }

        state = radio.setCrcFiltering(false);
        if (state != RADIOLIB_ERR_NONE)
        {
            log_e("%s Error disabling crc filtering: [%d]", TRANSCEIVER_CHIP, state);
            while (true)
                delay(10);
        }

        // Preamble: AA AA AA AA AA
        // Sync is: 2D D4
        // Preamble 40 bits but the CC1101 doesn't allow us to set that
        // so we use a preamble of 32 bits and then use the sync as AA 2D
        // which then uses the last byte of the preamble - we recieve the last sync byte
        // as the 1st byte of the payload.
        uint8_t sync_word[] = {0xAA, 0x2D};
        state = radio.setSyncWord(sync_word, 2);

        if (state != RADIOLIB_ERR_NONE)
        {
            log_e("%s Error setting sync words: [%d]", TRANSCEIVER_CHIP, state);
            while (true)
                delay(10);
        }
    }
    else
    {
        log_e("%s Error initialising: [%d]", TRANSCEIVER_CHIP, state);
        while (true)
            delay(10);
    }
    log_d("%s Setup complete - awaiting incoming messages...", TRANSCEIVER_CHIP);
    rssi = radio.getRSSI();

    // Set callback function
    radio.setPacketReceivedAction(setFlag);

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE)
    {
        log_e("%s startReceive() failed, code %d", TRANSCEIVER_CHIP, state);
        while (true)
            delay(10);
    }

    return state;
}

DecodeStatus decodeMessage(const uint8_t *msg, uint8_t msgSize)
{
    DecodeStatus decode_res = DECODE_INVALID;

    // | byte0  | byte1  | byte2 | ... | byteN |
    // |--------|--------|-------|-----|-------|
    // | digest | digest |    <- payload ->    |
    // | [15:8] |  [7:0] |                     |
    // | <------------- whitening -----------> |

    // data de-whitening
    uint8_t msgw[MSG_BUF_SIZE];
    for (unsigned i = 0; i < msgSize; ++i)
    {
        msgw[i] = msg[i] ^ 0xaa;
    }

    // LFSR-16 digest, generator 0x8005 key 0xba95 final xor 0x6df1
    int chkdgst = (msgw[0] << 8) | msgw[1];
    int digest = lfsr_digest16(&msgw[2], 27, 0x8005, 0xba95);
    if ((chkdgst ^ digest) != 0x6df1)
    {
        log_d("Digest check failed - [%04X] vs [%04X] (%04X)", chkdgst, digest, chkdgst ^ digest);
        return DECODE_DIG_ERR;
    }

#if CORE_DEBUG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    log_message("De-whitened Data", msgw, msgSize);
#endif

    // --- DECODE PAYLOAD TO STRUCT ---
    // Payload format must match getPayloadStage2() in AppLayer.cpp
    // For port == 1:
    // [uint8_t result][uint8_t status][uint8_t faultcode][float energytoday][float energytotal]
    // [float totalworktime][float outputpower][float gridvoltage][float gridfrequency]

    int offset = 2; // skip digest bytes

    uint8_t result = msgw[offset++];
    if (result != 0) {
        log_e("Payload result error: %u", result);
        return DECODE_INVALID;
    }

    modbusdata.status = msgw[offset++];
    modbusdata.faultcode = msgw[offset++];

    memcpy(&modbusdata.energytoday,   &msgw[offset], sizeof(float)); offset += sizeof(float);
    memcpy(&modbusdata.energytotal,   &msgw[offset], sizeof(float)); offset += sizeof(float);
    memcpy(&modbusdata.totalworktime, &msgw[offset], sizeof(float)); offset += sizeof(float);
    memcpy(&modbusdata.outputpower,   &msgw[offset], sizeof(float)); offset += sizeof(float);
    memcpy(&modbusdata.gridvoltage,   &msgw[offset], sizeof(float)); offset += sizeof(float);
    memcpy(&modbusdata.gridfrequency, &msgw[offset], sizeof(float)); offset += sizeof(float);

    // --- Convert modbusdata to JSON ---
    JsonDocument doc;
    doc["status"]        = modbusdata.status;
    doc["faultcode"]     = modbusdata.faultcode;
    doc["energytoday"]   = modbusdata.energytoday;
    doc["energytotal"]   = modbusdata.energytotal;
    doc["totalworktime"] = modbusdata.totalworktime;
    doc["outputpower"]   = modbusdata.outputpower;
    doc["gridvoltage"]   = modbusdata.gridvoltage;
    doc["gridfrequency"] = modbusdata.gridfrequency;

    char json[256];
    serializeJson(doc, json, sizeof(json));
    log_i("Decoded JSON: %s", json);

    decode_res = DECODE_OK;
    return decode_res;
}

DecodeStatus getMessage(void)
{
    uint8_t recvData[MSG_BUF_SIZE];
    DecodeStatus decode_res = DECODE_INVALID;

    // Receive data
    if (receivedFlag)
    {
        receivedFlag = false;

        int state = radio.readData(recvData, MSG_BUF_SIZE);
        rssi = radio.getRSSI();

        if (state == RADIOLIB_ERR_NONE)
        {
            // Verify last syncword is 1st byte of payload (see setSyncWord() above)
            if (recvData[0] == 0xD4)
            {
#if CORE_DEBUG_LEVEL == ARDUHAL_LOG_LEVEL_VERBOSE
                char buf[128];
                *buf = '\0';
                for (size_t i = 0; i < sizeof(recvData); i++)
                {
                    sprintf(&buf[strlen(buf)], "%02X ", recvData[i]);
                }
                log_v("%s Data: %s", TRANSCEIVER_CHIP, buf);
#endif
                log_d("%s R [%02X] RSSI: %0.1f", TRANSCEIVER_CHIP, recvData[0], rssi);

                decode_res = decodeMessage(&recvData[1], sizeof(recvData) - 1);
            } // if (recvData[0] == 0xD4)
        } // if (state == RADIOLIB_ERR_NONE)
        else if (state == RADIOLIB_ERR_RX_TIMEOUT)
        {
            log_v("T");
        }
        else
        {
            // some other error occurred
            log_d("%s Receive failed: [%d]", TRANSCEIVER_CHIP, state);
        }
    }

    return decode_res;
}

bool getData(uint32_t timeout, void (*func)())
{
    const uint32_t timestamp = millis();

    radio.startReceive();

    while ((millis() - timestamp) < timeout)
    {
        int decode_status = getMessage();

        // Callback function (see https://www.geeksforgeeks.org/callbacks-in-c/)
        if (func)
        {
            (*func)();
        }

        if (decode_status == DECODE_OK)
        {
            radio.standby();
            return true;
        } // if (decode_status == DECODE_OK)
    } //  while ((millis() - timestamp) < timeout)

    // Timeout
    radio.standby();
    return false;
}

void setup()
{
    // Initialize Serial for debugging
    Serial.begin(115200);

    setupRadio();

    bool valid = getData(RX_TIMEOUT, NULL);
    if (!valid)
    {
        log_e("Failed to get data within timeout.");
    }
    ESP.deepSleep(SLEEP_INTERVAL * 1000);
}

void loop()
{
}