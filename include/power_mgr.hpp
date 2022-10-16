#pragma once
#include <stdint.h>
class power_mgr {
public:
    void initialize();
    bool initialized() const;
    bool charging() const;
    bool charged() const;
    float level() const;
};