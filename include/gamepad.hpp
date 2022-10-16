#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>
struct gamepad_buttons {
    uint16_t start : 1;
    uint16_t select : 1;
    uint16_t up : 1;
    uint16_t down : 1;
    uint16_t left : 1;
    uint16_t right : 1;
    uint16_t a : 1;
    uint16_t b : 1;
    uint16_t menu : 1;
    uint16_t l : 1;
    uint16_t r : 1;
};
class gamepad {
    constexpr static const uint8_t l_pin = 36;
    constexpr static const uint8_t r_pin = 34;
    constexpr static const uint8_t menu_pin = 35;
    bool m_initialized;
    TwoWire& m_i2c;
public:
    gamepad(TwoWire& i2c=Wire);
    bool initialized() const;
    void initialize();
    gamepad_buttons read() const;
};