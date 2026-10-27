#pragma once
#include <cstdint>
inline uint32_t fake_millis = 0;
inline uint32_t millis() { return fake_millis; }
inline void delay(uint32_t value) { fake_millis += value; }
