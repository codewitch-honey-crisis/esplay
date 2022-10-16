#include <gamepad.hpp>
#include <Arduino.h>
constexpr static const uint8_t gamepad_i2c_address = 0x20;
gamepad::gamepad(TwoWire& i2c) : m_initialized(false), m_i2c(i2c) {

}
bool gamepad::initialized() const {
    return m_initialized;
}
void gamepad::initialize() {
    if(!m_initialized) {
        m_i2c.begin();
        pinMode(l_pin,INPUT_PULLUP);
        pinMode(r_pin,INPUT_PULLUP);
        pinMode(menu_pin,INPUT_PULLUP);
        
        m_initialized = true;
    }
}
gamepad_buttons gamepad::read() const {
    gamepad_buttons r = {0};
    if(initialized()) {   
        m_i2c.requestFrom(gamepad_i2c_address,uint8_t(1));
        uint8_t rr=m_i2c.read();
        m_i2c.endTransmission(true);
        r.start = !((1<<0)&rr);
        r.select = !((1<<1)&rr);
        r.up = !((1<<2)&rr);
        r.down = !((1<<3)&rr);
        r.left = !((1<<4)&rr);
        r.right = !((1<<5)&rr);
        r.a = !((1<<6)&rr);
        r.b = !((1<<7)&rr);
        r.l = !digitalRead(l_pin);
        r.r = !digitalRead(r_pin);
        r.menu = !digitalRead(menu_pin);
    }
    return r;
}