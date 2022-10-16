#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <gamepad.hpp>
#include <power_mgr.hpp>
#include <SD_MMC.h>
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
  File root = SD_MMC.open("/","r");
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
  // backlight
  pinMode(27,OUTPUT);
  digitalWrite(27,HIGH);
  SD_MMC.begin("/sdcard",true);
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

void loop() {
  //gamepad_buttons b = input.read();
 // if(b.b) {
  //  Serial.println("b");
  //}
}