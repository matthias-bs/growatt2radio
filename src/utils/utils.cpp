///////////////////////////////////////////////////////////////////////////////////////////////////
// utils.cpp
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
// 20250709 Created from https://github.com/matthias-bs/growatt2lorawan-v2
//
// ToDo:
// -
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "utils.h"

//
// From from rtl_433 project - https://github.com/merbanan/rtl_433/blob/master/src/util.c
//
uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key)
{
  uint16_t sum = 0;
  for (unsigned k = 0; k < bytes; ++k)
  {
    uint8_t data = message[k];
    for (int i = 7; i >= 0; --i)
    {
      // fprintf(stderr, "key at bit %d : %04x\n", i, key);
      // if data bit is set then xor with key
      if ((data >> i) & 1)
        sum ^= key;

      // roll the key right (actually the lsb is dropped here)
      // and apply the gen (needs to include the dropped lsb as msb)
      if (key & 1)
        key = (key >> 1) ^ gen;
      else
        key = (key >> 1);
    }
  }
  return sum;
}

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
void log_message(const char *descr, const uint8_t *msg, uint8_t msgSize)
{
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
    for (size_t i = 0; i < msgSize; i++)
    {
        sprintf(&buf[strlen(buf)], "%02d ", i);
    }
    log_d("%s", buf);

    memset(buf, ' ', prefix_len);
    buf[prefix_len] = '\0';
    offs = (len1 > len2) ? (len1 - len2) : 0;
    sprintf(&buf[offs], "%s: ", descr);

    for (size_t i = 0; i < msgSize; i++)
    {
        sprintf(&buf[strlen(buf)], "%02X ", msg[i]);
    }
    log_d("%s", buf);
}
#endif
