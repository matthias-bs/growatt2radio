#if !defined(UTILS_H)
#define UTILS_H 
#include <Arduino.h>

uint16_t lfsr_digest16(uint8_t const message[], unsigned bytes, uint16_t gen, uint16_t key);

#endif