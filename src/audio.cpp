#include <audio.hpp>
#include <Arduino.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#define I2S_BCK GPIO_NUM_26
#define I2S_WS GPIO_NUM_25
#define I2S_DOUT GPIO_NUM_19
#define AMP_SHDN GPIO_NUM_4
#define VOLUME_LEVEL_COUNT 100

static float Volume = 1.0f;
static int volumeLevel = 100;
static int sampleRate = 0;


extern "C" void audio_amp_enable()
{
    Serial.println("Amp enabled");
    digitalWrite(AMP_SHDN, HIGH);
}

extern "C" void audio_amp_disable()
{
    Serial.println("Amp disabled");
    digitalWrite(AMP_SHDN, LOW);
}

extern "C" int audio_volume_get()
{
    return volumeLevel;
}

extern "C" void audio_volume_set(int value)
{
    if (value > VOLUME_LEVEL_COUNT)
    {
        printf("audio_volume_set: value out of range (%d)\n", value);
        abort();
    }

    volumeLevel = value;
    Volume = (float)(volumeLevel*10) * 0.001f;

    if (volumeLevel == 0)
        audio_amp_disable();
}
#define SAMPLE_RATE 32000
#define I2S_NUM I2S_NUM_0
extern "C" void audio_init(int sample_rate)
{
    i2s_config_t i2s_config;
    i2s_pin_config_t pins;
    memset(&i2s_config,0,sizeof(i2s_config_t));
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = sample_rate;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_MSB | I2S_COMM_FORMAT_STAND_I2S);
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 534;
    i2s_config.use_apll = true;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_driver_install((i2s_port_t)I2S_NUM, &i2s_config, 0, NULL);
    pins = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = 26,
        .ws_io_num = 25,
        .data_out_num = 19,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
  i2s_set_pin((i2s_port_t)I2S_NUM, &pins);
    // NOTE: buffer needs to be adjusted per AUDIO_SAMPLE_RATE

    sampleRate = sample_rate;
    int32_t vol = 10;
    pinMode(AMP_SHDN,OUTPUT);
    audio_volume_set(vol);
 
    if(volumeLevel != 0)
        audio_amp_enable();
}

extern "C" void audio_submit(short *stereoAudioBuffer, int frameCount)
{
    
    if (volumeLevel != 0)
    {
        short currentAudioSampleCount = frameCount * 2;
        
        for (short i = 0; i < currentAudioSampleCount; ++i)
        {
            int sample = stereoAudioBuffer[i] * Volume;
            
            stereoAudioBuffer[i] = (short)sample;
        }

        int len = currentAudioSampleCount * sizeof(int16_t);
        size_t count;
        size_t toWrite = len;
        char* p = (char*)stereoAudioBuffer;
        while(toWrite) {
            len=toWrite;
            if(len>534) {
                len=534;
            }
            i2s_write(I2S_NUM_0, p, len, &count, portMAX_DELAY);
            if (count != len)
            {
                printf("i2s_write_bytes: count (%d) != len (%d)\n", count, len);
                abort();
            }
            p+=count;
            toWrite-=count;
        }
        
    }
}

extern "C" void audio_terminate()
{
    audio_amp_disable();
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_stop(I2S_NUM_0);

    i2s_start(I2S_NUM_0);
}

extern "C" void audio_resume()
{
    if (volumeLevel != 0)
        audio_amp_enable();
}
