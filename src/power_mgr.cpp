#include <power_mgr.hpp>
#include "driver/rtc_io.h"
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "esp_system.h"
#include "esp_event.h"

#define USB_PLUG_PIN GPIO_NUM_32
#define CHRG_STATE_PIN GPIO_NUM_33
#define ADC_PIN ADC1_CHANNEL_3
#define LED1 GPIO_NUM_13

#define DEFAULT_VREF 1100

typedef struct
{
	int millivolts;
	int percentage;
} battery_state;

typedef enum
{
  NO_CHRG = 0,
	CHARGING,
	FULL_CHARGED
} charging_state;

static esp_adc_cal_characteristics_t characteristics;
static bool input_battery_initialized = false;
static float adc_value = 0.0f;
static float forced_adc_value = 0.0f;
static bool battery_monitor_enabled = true;
static void battery_level_read(battery_state *out_state)
{
    if (!input_battery_initialized)
    {
        printf("battery_level_read: not initilized.\n");
        abort();
    }

    const int sampleCount = 8;

    float adcSample = 0.0f;
    for (int i = 0; i < sampleCount; ++i)
    {
        //adcSample += adc1_to_voltage(ADC1_CHANNEL_0, &characteristics) * 0.001f;
        adcSample += esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC_PIN), &characteristics) * 0.001f;
    }
    adcSample /= sampleCount;

    if (adc_value == 0.0f)
    {
        adc_value = adcSample;
    }
    else
    {
        adc_value += adcSample;
        adc_value /= 2.0f;
    }

    // Vo = (Vs * R2) / (R1 + R2)
    // Vs = Vo / R2 * (R1 + R2)
    const float R1 = 100000;
    const float R2 = 100000;
    const float Vo = adc_value;
    const float Vs = (forced_adc_value > 0.0f) ? (forced_adc_value) : (Vo / R2 * (R1 + R2));

    const float FullVoltage = 4.1f;
    const float EmptyVoltage = 3.4f;

    out_state->millivolts = (int)(Vs * 1000);
    out_state->percentage = (int)((Vs - EmptyVoltage) / (FullVoltage - EmptyVoltage) * 100.0f);
    if (out_state->percentage > 100)
        out_state->percentage = 100;
    if (out_state->percentage < 0)
        out_state->percentage = 0;
}
void power_mgr::initialize() {
    if(!input_battery_initialized) {
        
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[LED1], PIN_FUNC_GPIO);
    gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
    gpio_set_direction(USB_PLUG_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(USB_PLUG_PIN, GPIO_PULLUP_ONLY);
    gpio_set_direction(CHRG_STATE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CHRG_STATE_PIN, GPIO_PULLUP_ONLY);
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC_PIN, ADC_ATTEN_11db);

    //int vref_value = odroid_settings_VRef_get();
    //esp_adc_cal_get_characteristics(vref_value, ADC_ATTEN_11db, ADC_WIDTH_12Bit, &characteristics);

    //Characterize ADC
    //adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, DEFAULT_VREF, &characteristics);
    
    input_battery_initialized = true;
    }
}
bool power_mgr::initialized() const {
    return input_battery_initialized;
}
bool power_mgr::charging() const {
    if(input_battery_initialized) {
        if(!gpio_get_level(USB_PLUG_PIN))
            return false;
        else
        {
            if(!gpio_get_level(CHRG_STATE_PIN))
                return true;
            else
                return false;
        }
    }
    return false;
}
bool power_mgr::charged() const {
     if(input_battery_initialized) {
        if(!gpio_get_level(USB_PLUG_PIN))
            return false;
        else
        {
            if(!gpio_get_level(CHRG_STATE_PIN))
                return false;
            else
                return true;
        }
    }
    return false;
}
float power_mgr::level() const {
    battery_state s;
    battery_level_read(&s);
    return s.percentage/100.0f;
}