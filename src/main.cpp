#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <gamepad.hpp>
#include <power_mgr.hpp>
#include <audio.hpp>
#include <SD_MMC.h>


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

using namespace fs;
extern "C"
{
#include <nofrendo.h>
}

TFT_eSPI tft;
gamepad input;
power_mgr power;
int16_t bg_color;
uint16_t myPallete[256];
void display_list(const char* list, int start_index, int selected_index) {
  tft.fillScreen(0);
  const char* sz = list;
  tft.setTextSize(2);
  int i = 0;
  int h = 4;
  int j = 0;
  char fn[256];
  while(*sz && j<14) {
    if(i<start_index) {
      ++i;
      sz+=(strlen(sz)+1);
      continue;
    }
    if(i==selected_index) {
      tft.setTextColor(TFT_RED);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(4,h);
    strcpy(fn,sz);
    int l = strlen(fn)-4;
    if(l>16) l = 16;
    fn[l]='\0';
    tft.print(fn);
    h+=16;
    sz+=(strlen(sz)+1);
    ++i;
    ++j;
  }

}
char* load_file_list() {
  char* result = nullptr;
  File root = SD_MMC.open("/","rb");
  File file = root.openNextFile();
  size_t total_len = 0;  
  while (file)
  {
      if (file.isDirectory())
      {
          // skip
      }
      else
      {
          char *filename = (char *)file.name();
          int8_t len = strlen(filename);
          if (0==strcasecmp(filename + (len - 4), ".nes"))
          {
              char sz[5];
              file.seek(0);
              if(4==file.read((uint8_t*)sz,4)) {
                sz[4]='\0';
                if(0==strcmp(sz,"NES\x1A")) {
                  if(result==nullptr) {
                    result = (char*)malloc(len+1);
                    if(result==nullptr) {
                      Serial.println("Out of memory loading files");
                      while(true);
                    }
                  } else {
                      result = (char*)realloc(result,total_len+len+1);
                      if(result==nullptr) {
                        Serial.println("Out of memory loading files");
                        while(true);
                      }
                  }
                  Serial.println(filename);
                  strcpy(result+total_len,filename);
                  total_len+=(len+1);
                  result[total_len-1]='\0';
                } else {
                  Serial.printf("Invalid fourcc: %x %x %x %x\n",sz[0],sz[1],sz[2],sz[3]);
                }
              }
          }
      }
      file.close();
      file = root.openNextFile();
  }
  if(result==nullptr) {
    result = (char*)malloc(1);
    if(result==nullptr) {
      Serial.println("Out of memory loading files");
      while(true);
    }
  } else {
      result = (char*)realloc(result,total_len+1);
      if(result==nullptr) {
        Serial.println("Out of memory loading files");
        while(true);
      }
  }
  result[total_len]='\0';
  return result;
}

void setup() {
  char *argv[1];
  Wire.begin(21,22,uint32_t(100000));
  Serial.begin(115200);
  power.initialize();
  input.initialize();
  SD_MMC.begin("/sdcard",true);
  /*
  audio_init(32000);
  audio_volume_set(100);
  audio_amp_enable();
 */
  // backlight
  pinMode(27,OUTPUT);
  digitalWrite(27,HIGH);
  
  Serial.printf("SD size %02fMB\n",SD_MMC.cardSize()/1024.0/1024.0);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  char* list = load_file_list();
  int list_count = 0;
  const char* sz = list;
  while(*sz) {
    sz+=(strlen(sz)+1);
    ++list_count;
  }
  int selected_index = 0;
  int start_index = 0;
  gamepad_buttons ob = {0};
  gamepad_buttons b = {0};
  char filename[256];
  sprintf(filename,"%s%s","/sdcard/",list);
  while(!b.a && !b.b && !b.select && !b.start) {
    display_list(list,start_index,selected_index);
    b=input.read();
    while(ob.up==b.up && ob.down==b.down && !b.a && !b.b && !b.select && !b.start) {
      ob=b;
      b = input.read();  
      if(b.menu) {
        ESP.restart();
      }
      delay(1);
    }
    delay(100);
    if(b.down && !ob.down) {
      if(selected_index<list_count-1) {
        if(selected_index-start_index==13) {
          ++selected_index;
          ++start_index;
        } else {
          ++selected_index;
        }
      }
    } else if(b.up && !ob.up) {
      if(selected_index) {
        if(selected_index==start_index) {
          --start_index;
          --selected_index;
        } else {
          --selected_index;
        }
      }
    }
  }
  int i=0;
  sz = list;
  while(i<selected_index) {
    sz += (strlen(sz)+1);
    ++i;
  }
  Serial.print("File: ");
  Serial.println(sz);
  sprintf(filename,"%s%s","/sdcard/",sz);
  free(list);
  argv[0]=filename;
  nofrendo_main(1,argv);
}

#define SAMPLE_RATE     (32000)
#define SAMPLE_RATE_INPUT     (32000)
#define DMA_BUF_LEN     (512)
#define DMA_BUF_LEN_INPUT     (512)
#define DMA_NUM_BUF     (8)
#define TWOPI           (6.28318531f)
#define I2S_NUM I2S_NUM_0
//static short audio_out_buf[DMA_BUF_LEN];
// Accumulated phase
//static float phase = 0.0f;
//const float pdc = TWOPI*500/SAMPLE_RATE;
//float pd = pdc;

void loop() {
  /*float f;
  int16_t samp;
  for (int i=0; i < DMA_BUF_LEN; i++) {
    f = (sinf(phase) + 1.0f) * 0.5f;
    // Increment and wrap phase
    phase += pdc;
    if (phase >= TWOPI) {
        phase -= TWOPI;
    }
    samp = f* 65535.0f * .5;
    audio_out_buf[i] = samp ;
    audio_submit(audio_out_buf,DMA_BUF_LEN/2);
  }*/

}