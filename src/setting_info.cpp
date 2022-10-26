#include <setting_info.hpp>
#include <Arduino.h>
#include <SPIFFS.h>
void setting_info::load(setting_info* settings) {
    SPIFFS.begin();
    if(!SPIFFS.exists("/settings")) {
        settings->save_slot = 0;
        settings->volume = 30;
        return;
    }
    File file = SPIFFS.open("/settings","rb");
    file.read((uint8_t*)settings,sizeof(setting_info));
    file.close();
}
void setting_info::save(const setting_info& settings) {
    SPIFFS.begin();
    File file = SPIFFS.open("/settings","wb",true);
    file.write((uint8_t*)&settings,sizeof(setting_info));
    file.close();
}