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
// 20250709 Fixed sleep interval, RSSI type and retry reception
//          Added ESP32 chip_id as transmitter ID to message to allow multiple transceivers
// 20250710 Added inverter temperature to port 1 payload
//          Removed TLS fingerprint option (insecure)
//          Added Modbus status to JSON string
// 20250802 Fixed MQTT status message topic and disconnect timing
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <RadioLib.h>
#include <LoraEncoder.h>
#include <ArduinoJson.h>
#include <MQTT.h>
#include <growatt_cfg.h>
#include <utils/utils.h>
#include "gw_receiver.h"

#define SLEEP_INTERVAL 300      // sleep interval in seconds
#define SLEEP_INTERVAL_SHORT 10 // sleep interval in seconds if receive failed
#define RX_TIMEOUT 180000       // sensor receive timeout [ms]
#define TRANSMITTER_ID 0        // 32-bit transmitter ID; 0 - allow any ID
#define MSG_BUF_SIZE 36         // last byte of preamble + digest (2 Bytes) + tx_id (4 Bytes)
                                // + payload (29 Bytes)
#define MQTT_PAYLOAD_SIZE 256   // define the payload size for MQTT messages
#define TIMEZONE 1              // UTC + TIMEZONE
// Enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
const char *TZ_INFO = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
#define WIFI_RETRIES 10 // WiFi connection retries
#define WIFI_DELAY 1000 // Delay between connection attempts [ms]

#define USE_WIFI
// #define USE_SECUREWIFI

// enable only one of these below, disabling both is fine too.
#define CHECK_CA_ROOT
//  #define CHECK_PUB_KEY
////--------------------------////

#if (defined(USE_SECUREWIFI) && defined(USE_WIFI)) || (!defined(USE_SECUREWIFI) && !defined(USE_WIFI))
#error "Either USE_SECUREWIFI OR USE_WIFI must be defined!"
#endif

#include <WiFi.h>
#if defined(USE_SECUREWIFI)
#include <NetworkClientSecure.h>
#endif

#include "secrets.h"

#ifndef SECRETS
const char ssid[] = "WiFiSSID";
const char pass[] = "WiFiPassword";

const char HOSTNAME[] = "ESPWeather";
#define APPEND_CHIP_ID

#define MQTT_PORT 8883 // checked by pre-processor!
const char MQTT_HOST[] = "xxx.yyy.zzz.com";
const char MQTT_USER[] = ""; // leave blank if no credentials used
const char MQTT_PASS[] = ""; // leave blank if no credentials used

#ifdef CHECK_CA_ROOT
static const char digicert[] PROGMEM = R"EOF(
    -----BEGIN CERTIFICATE-----
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    -----END CERTIFICATE-----
    )EOF";
#endif

#ifdef CHECK_PUB_KEY
// Extracted by: openssl x509 -pubkey -noout -in fullchain.pem
static const char pubkey[] PROGMEM = R"KEY(
    -----BEGIN PUBLIC KEY-----
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    xxxxxxxx
    -----END PUBLIC KEY-----
    )KEY";
#endif
#endif

// MQTT topics - change if needed
String Hostname = String(HOSTNAME);
String mqttPubStatus = "status";
String mqttPubData = "data";
String mqttPubRssi = "rssi";

static char json[MQTT_PAYLOAD_SIZE];

// Generate WiFi network instance
#if defined(USE_WIFI)
WiFiClient net;
#elif defined(USE_SECUREWIFI)
NetworkClientSecure net;
#endif

//
// Generate MQTT client instance
//
MQTTClient client(MQTT_PAYLOAD_SIZE);

static float rssi = 0; // variable to hold the RSSI value

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

/*!
 * \brief Set RTC
 *
 * \param epoch Time since epoch
 * \param ms unused
 */
void setTime(unsigned long epoch, int ms)
{
    struct timeval tv;

    if (epoch > 2082758399)
    {
        tv.tv_sec = epoch - 2082758399; // epoch time (seconds)
    }
    else
    {
        tv.tv_sec = epoch; // epoch time (seconds)
    }
    tv.tv_usec = ms; // microseconds
    settimeofday(&tv, NULL);
}

/// Print date and time (i.e. local time)
void printDateTime(void)
{
    struct tm timeinfo;
    char tbuf[25];

    time_t tnow;
    time(&tnow);
    localtime_r(&tnow, &timeinfo);
    strftime(tbuf, 25, "%Y-%m-%d %H:%M:%S", &timeinfo);
    log_i("%s", tbuf);
}

/*!
 * \brief Wait for WiFi connection
 *
 * \param wifi_retries   max. no. of retries
 * \param wifi_delay    delay in ms before each attemüt
 */
void wifi_wait(int wifi_retries, int wifi_delay)
{
    int count = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(wifi_delay);
        if (++count == wifi_retries)
        {
            log_e("\nWiFi connection timed out, will restart after %d s", SLEEP_INTERVAL / 1000);
            ESP.deepSleep(SLEEP_INTERVAL * 1000);
        }
    }
}

/*!
 * \brief WiFiManager Setup
 *
 * Configures WiFi access point and MQTT connection parameters
 */
void mqtt_setup(void)
{
    log_i("Attempting to connect to SSID: %s", ssid);
    WiFi.hostname(Hostname.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    wifi_wait(WIFI_RETRIES, WIFI_DELAY);
    log_i("connected!");

    // Note: TLS security and rain/lightning statistics need correct time
    log_i("Setting time using SNTP");
    configTime(TIMEZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    int retries = 10;
    while (now < 1510592825)
    {
        if (--retries == 0)
            break;
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }
    if (retries == 0)
    {
        log_w("\nSetting time using SNTP failed!");
    }
    else
    {
        log_i("\ndone!");
        setTime(time(nullptr), 0);
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    log_i("Current time (GMT): %s", asctime(&timeinfo));

#ifdef USE_SECUREWIFI
#if defined(ESP8266)
#ifdef CHECK_CA_ROOT
    BearSSL::X509List cert(digicert);
    net.setTrustAnchors(&cert);
#endif
#ifdef CHECK_PUB_KEY
    BearSSL::PublicKey key(pubkey);
    net.setKnownKey(&key);
#endif
#elif defined(ESP32)
#ifdef CHECK_CA_ROOT
    net.setCACert(digicert);
#endif
#ifdef CHECK_PUB_KEY
    error "CHECK_PUB_KEY: not implemented"
#endif
#endif
#if (!defined(CHECK_PUB_KEY) and !defined(CHECK_CA_ROOT))
    // do not verify tls certificate
    net.setInsecure();
#endif
#endif
    client.begin(MQTT_HOST, MQTT_PORT, net);
    client.setWill(mqttPubStatus.c_str(), "dead", true /* retained */, 1 /* qos */);
    mqtt_connect();
}

/*!
 * \brief (Re-)Connect to WLAN and connect MQTT broker
 */
void mqtt_connect(void)
{
    Serial.print(F("Checking wifi..."));
    wifi_wait(WIFI_RETRIES, WIFI_DELAY);

    Serial.print(F("\nMQTT connecting... "));
    while (!client.connect(Hostname.c_str(), MQTT_USER, MQTT_PASS))
    {
        Serial.print(".");
        delay(1000);
    }

    log_i("%s: %s\n", mqttPubStatus.c_str(), "online");
    client.publish(mqttPubStatus, "online");
}

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
    // | preamble | byte0  | byte1  | byte2   | byte3   | byte4   | byte5   | byte6 | ... | byteN |
    // | -------- |--------|--------|---------|---------|---------|---------|-------|-----|-------|
    // |          | digest | digest | chip_id | chip_id | chip_id | chip_id |    <- payload ->    |
    // |          | [15:8] |  [7:0] | [31:24] | [23:16] |  [15:8] |   [7:0] |     (uplinkSize)    |
    // |          | <------------- whitening ---------------------------------------------------> |

    // data de-whitening
    uint8_t msgw[MSG_BUF_SIZE];
    for (unsigned i = 0; i < msgSize; ++i)
    {
        msgw[i] = msg[i] ^ 0xaa;
    }

    // LFSR-16 digest, generator 0x8005 key 0xba95 final xor 0x6df1
    int chkdgst = (msgw[0] << 8) | msgw[1];
    int digest = lfsr_digest16(&msgw[2], msgSize - 2, 0x8005, 0xba95);
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
    uint32_t transmitter_id = 0;
    for (int i = 0; i < 4; i++)
    {
        transmitter_id |= (msgw[offset++] << (24 - i * 8));
    }
    log_i("Transmitter ID: %08lX", transmitter_id);

    if (TRANSMITTER_ID != 0 && TRANSMITTER_ID != transmitter_id)
    {
        log_i("Transmitter ID mismatch: expected %08lX, got %08lX", TRANSMITTER_ID, transmitter_id);
        return DECODE_INVALID;
    }

    uint8_t result = msgw[offset++];
    if (result != 0)
    {
        log_e("Modbus error: %u", result);
    }

    JsonDocument doc;
    doc["modbus"] = result;

    if (result == 0)
    {
        modbusdata.status = msgw[offset++];
        modbusdata.faultcode = msgw[offset++];

        memcpy(&modbusdata.energytoday, &msgw[offset], sizeof(float));
        offset += sizeof(float);
        memcpy(&modbusdata.energytotal, &msgw[offset], sizeof(float));
        offset += sizeof(float);
        memcpy(&modbusdata.totalworktime, &msgw[offset], sizeof(float));
        offset += sizeof(float);
        memcpy(&modbusdata.outputpower, &msgw[offset], sizeof(float));
        offset += sizeof(float);
        memcpy(&modbusdata.gridvoltage, &msgw[offset], sizeof(float));
        offset += sizeof(float);
        memcpy(&modbusdata.gridfrequency, &msgw[offset], sizeof(float));
        offset += sizeof(float);
        // Decode tempinverter (2 bytes, two's complement)
        int16_t encodedTemp = (msgw[offset++] << 8) | msgw[offset++]; // Combine high and low bytes
        modbusdata.tempinverter = encodedTemp / 100.0;                // Reverse scaling by dividing by 100

        // --- Convert modbusdata to JSON ---
        doc["status"] = modbusdata.status;
        doc["faultcode"] = modbusdata.faultcode;
        doc["energytoday"] = modbusdata.energytoday;
        doc["energytotal"] = modbusdata.energytotal;
        doc["totalworktime"] = modbusdata.totalworktime;
        doc["outputpower"] = modbusdata.outputpower;
        doc["gridvoltage"] = modbusdata.gridvoltage;
        doc["gridfrequency"] = modbusdata.gridfrequency;
        doc["tempinverter"] = modbusdata.tempinverter;
    }

    serializeJson(doc, json, sizeof(json));
    log_i("Decoded JSON: %s", json);

    return DECODE_OK;
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
        radio.startReceive();

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
        DecodeStatus decode_status = getMessage();

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
        else
        {
            if (decode_status == DECODE_DIG_ERR)
            {
                log_d("Digest error, retrying...");
            }
            else if (decode_status != DECODE_INVALID)
            {
                log_d("Unknown decode status: %d", decode_status);
            }
        }
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
        log_i("Sleeping for %d s\n", SLEEP_INTERVAL_SHORT);
        ESP.deepSleep(SLEEP_INTERVAL_SHORT * 1000000L);
    }

    // Set time zone
    setenv("TZ", TZ_INFO, 1);
    printDateTime();

#ifdef LED_EN
    // Configure LED output pins
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, HIGH);
#endif

    char ChipID[8] = "";

#if defined(APPEND_CHIP_ID) && defined(ESP32)
    uint32_t chip_id = 0;
    for (int i = 0; i < 17; i = i + 8)
    {
        chip_id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    sprintf(ChipID, "-%06lX", chip_id);
#elif defined(APPEND_CHIP_ID) && defined(ESP8266)
    sprintf(ChipID, "-%06lX", ESP.getChipId() & 0xFFFFFF);
#endif

    Hostname = Hostname + ChipID;
    // Prepend Hostname to MQTT topics
    mqttPubData = Hostname + "/" + mqttPubData;
    mqttPubRssi = Hostname + "/" + mqttPubRssi;
    mqttPubStatus = Hostname + "/" + mqttPubStatus;

    mqtt_setup();

    log_i("%s: %s\n", mqttPubData.c_str(), json);
    client.publish(mqttPubData, json, false /* retain */, 0);

    log_i("%s: %0.1f", mqttPubRssi.c_str(), rssi);
    client.publish(mqttPubRssi, String(rssi, 1), false, 0);
    client.loop();

    log_i("Sleeping for %d s\n", SLEEP_INTERVAL);
    log_i("%s: %s\n", mqttPubStatus.c_str(), "offline");
    Serial.flush();
    client.publish(mqttPubStatus, "offline", true /* retained */, 0 /* qos */);
    for (int i = 0; i < 5; i++)
    {
        client.loop();
        delay(500);
    }
    delay(1000); // Allow time for the client to disconnect properly
    client.disconnect();
    net.stop();

    ESP.deepSleep(SLEEP_INTERVAL * 1000000L);
}

void loop()
{
}
