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
