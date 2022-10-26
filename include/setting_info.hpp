#pragma once
#include <stdint.h>
class setting_info {
public:
    uint8_t volume;
    uint8_t save_slot;
    static void load(setting_info* out_settings);
    static void save(const setting_info& settings);
};