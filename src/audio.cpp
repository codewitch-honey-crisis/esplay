#include <audio.hpp>

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
    gpio_set_level(AMP_SHDN, 1);
}

extern "C" void audio_amp_disable()
{
    gpio_set_level(AMP_SHDN, 0);
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

extern "C" void audio_init(int sample_rate)
{
    gpio_set_direction(AMP_SHDN, GPIO_MODE_OUTPUT);
    
    // NOTE: buffer needs to be adjusted per AUDIO_SAMPLE_RATE
    i2s_config_t i2s_config;
    i2s_config.fixed_mclk = 0;
    i2s_config.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT;
    i2s_config.tx_desc_auto_clear = 0;
    i2s_config.mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX); // Only TX
    i2s_config.sample_rate = sample_rate;
    i2s_config.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; //2-channels
    i2s_config.communication_format =(i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S | I2S_COMM_FORMAT_STAND_MSB);
    i2s_config.dma_buf_count = 8;
        //.dma_buf_len = 1472 / 2,  // (368samples * 2ch * 2(short)) = 1472
    i2s_config.dma_buf_len = 534;                       // (416samples * 2ch * 2(short)) = 1664
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1; //Interrupt level 1
    i2s_config.use_apll = 1;
        

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_pin_config_t pin_config;
    pin_config.mck_io_num = I2S_PIN_NO_CHANGE;
    pin_config.bck_io_num = I2S_BCK;
    pin_config.ws_io_num = I2S_WS;
    pin_config.data_out_num = I2S_DOUT;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE; //Not used
    
    i2s_set_pin(I2S_NUM_0, &pin_config);
    sampleRate = sample_rate;
    int32_t vol = volumeLevel;
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
            /*
            if (sample > 32767)
                sample = 32767;
            else if (sample < -32767)
                sample = -32767;
            */
            stereoAudioBuffer[i] = (short)sample;
        }

        int len = currentAudioSampleCount * sizeof(int16_t);
        size_t count;
        i2s_write(I2S_NUM_0, (const char *)stereoAudioBuffer, len, &count, portMAX_DELAY);
        if (count != len)
        {
            printf("i2s_write_bytes: count (%d) != len (%d)\n", count, len);
            abort();
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
