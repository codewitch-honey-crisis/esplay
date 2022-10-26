#include <Arduino.h>
#include <SPIFFS.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
extern "C" {
#include <noftypes.h>

#include <event.h>
#include <gui.h>
#include <log.h>
#include <nes/nes.h>
#include <nes/nes_pal.h>
#include <nes/nesinput.h>
#include <nofconfig.h>
#include <osd.h>
#include <nes/nesstate.h>
#include <driver/i2s.h>
}

#include <TFT_eSPI.h>
#include <setting_info.hpp>
#include <gamepad.hpp>
#include <power_mgr.hpp>
#include <audio.hpp>
#include <battery.h>

extern gamepad input;
extern power_mgr power;
extern setting_info settings;
extern TFT_eSPI tft;
TimerHandle_t timer;
SemaphoreHandle_t menu_sync;
static void draw_menu(int selected_index) {
    
    char sz[64];
    int x = 50, y=50;
    int w = TFT_WIDTH-100, h = TFT_HEIGHT-100;
    tft.fillRect(x,y,w,h,TFT_BLACK);
    x+=4; y+=4; w-=8; h-=8;

    tft.setTextSize(0);
    if(selected_index==0) {
        tft.setTextColor(TFT_RED);
    } else {
        tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(x,y);
    snprintf(sz,sizeof(sz),"volume: %d",((settings.volume+5)/10));
    tft.print(sz);
    y+=10;

    if(selected_index==1) {
        tft.setTextColor(TFT_RED);
    } else {
        tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(x,y);
    snprintf(sz,sizeof(sz),"save slot: %d",settings.save_slot);
    tft.print(sz);
    y+=10;
    
    if(selected_index==2) {
        tft.setTextColor(TFT_RED);
    } else {
        tft.setTextColor(TFT_WHITE);
    }
    tft.setCursor(x,y);
    strncpy(sz,"exit",sizeof(sz));
    tft.print(sz);
    y+=10;
    
}
static void start_menu() {
    audio_terminate();
    delay(100);
    
    xSemaphoreTake(menu_sync,portMAX_DELAY);
    gamepad_buttons ob = {0};
    gamepad_buttons b = {0};
    int selected = 0;
    while(!b.menu) {
        draw_menu(selected);
        b=input.read();
        while(!b.left && !b.right && !b.up && !b.down && !b.l && !b.r && !b.a && !b.b && !b.select && !b.start && !b.menu) {
            ob=b;
            b=input.read();
        }
        delay(100);
        if(selected>0 && b.up && !ob.up) {
            --selected;
            continue;
        }
        if(selected<2 && b.down && !ob.down) {
            ++selected;
            continue;
        }
        switch(selected) {
            case 0:
                if(settings.volume>0 && !ob.left&& b.left ) {
                    if(settings.volume<10) {
                        settings.volume = 0;
                    } else {
                        settings.volume-=10;
                    }
                    continue;
                }
                if(settings.volume<100 && !ob.right&& b.right ) {
                    if(settings.volume>90) {
                        settings.volume = 100;
                    } else {
                        settings.volume+=10;
                    }
                    continue;
                }
            break;
            case 1:
                if(settings.save_slot>0 && !ob.left&& b.left ) {
                    settings.save_slot-=1;
                    state_setslot(settings.save_slot);
                    continue;
                }
                if(settings.volume<9 && !ob.right&& b.right ) {
                    settings.save_slot+=1;
                    continue;
                }
            break;
            case 2:
                if((!ob.a && b.a) || !(ob.b || b.b) || (!ob.start && b.start)) {
                    ESP.restart();
                }
            break;
        }
    }
    int x = 50, y=50;
    int w = TFT_WIDTH-100, h = TFT_HEIGHT-100;
    tft.fillRect(x,y,w,h,TFT_BLACK);
    
    setting_info::save(settings);
    xSemaphoreGive(menu_sync);
    audio_resume();
}
/* memory allocation */
extern void *mem_alloc(int size, bool prefer_fast_memory)
{
	if (prefer_fast_memory)
	{
		return heap_caps_malloc(size, MALLOC_CAP_8BIT);
	}
	else
	{
		
		return heap_caps_malloc_prefer(size, MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT);
	}
}

/* sound */
static void (*audio_callback)(void *buffer, int length) = NULL;
static short *audio_frame=NULL;

extern "C" int osd_init_sound() {
    
    audio_frame = (short*)malloc(4 * 512);
    if(audio_frame==NULL) {
        return -1;
    }
    audio_init(32000);
 
    //audio_init(DEFAULT_SAMPLERATE);
    audio_callback = NULL;
    audio_volume_set(settings.volume);
    audio_amp_enable();
    return 0;
}
extern "C" void osd_stopsound() {
    audio_callback = NULL;
}

extern "C" void do_audio_frame()
{
    int left = 32000 / NES_REFRESH_RATE;
    while (left)
    {
        int n = 512;
        if (n > left)
            n = left;
        audio_callback(audio_frame, n);
        //16 bit mono -> 32-bit (16 bit r+l)
        for (int i = n - 1; i >= 0; i--)
        {
            int sample = (int)audio_frame[i];

            audio_frame[i * 2] = sample;
            audio_frame[i * 2 + 1] = sample;
        }
        audio_submit(audio_frame,n);
        left -= n;
    }

}

extern "C" void osd_setsound(void (*playfunc)(void *buffer, int length))
{
	//Indicates we should call playfunc() to get more data.
	audio_callback = playfunc;
}

extern "C" void osd_getsoundinfo(sndinfo_t *info) {
    info->sample_rate = 32000;
	info->bps = 16;
}

/* display */

static int16_t w, h, frame_x, frame_y, frame_x_offset, frame_width, frame_height, frame_line_pixels;
extern int16_t bg_color;
extern uint16_t myPalette[];

static void draw_battery(int x,int y,float level,bool color) {
  int l = (int)(level*14);
  tft.startWrite();
  tft.setAddrWindow(x,y,9,16);
  tft.pushPixels(battery,battery_size.width*battery_size.height);
  tft.endWrite();
  uint16_t col = TFT_GREEN;
  if(level<=.5) {
    col = TFT_YELLOW;
  } else if(level<=.25) {
    col = TFT_RED;
  }
  if(!color) {
    col = TFT_WHITE;
  }
  if(l>13) {
    if(l==14) {
      tft.drawFastHLine(x+3,y+1,3,col);  
      --l;
    }
    tft.drawFastHLine(x+3,y+2,3,col);
    --l;
  }
  tft.fillRect(x+1,y+2+(13-l),7,l,col);
}

static void display_begin()
{
    tft.begin();
    tft.setRotation(3);
    bg_color =  tft.color565(24, 28, 24); // DARK DARK GREY
    tft.fillScreen(bg_color);

}

static void display_init()
{
    w = tft.width();
    h = tft.height();
    if (w < 480) // assume only 240x240 or 320x240
    {
        if (w > NES_SCREEN_WIDTH)
        {
            frame_x = (w - NES_SCREEN_WIDTH) / 2;
            frame_x_offset = 0;
            frame_width = NES_SCREEN_WIDTH;
            frame_height = NES_SCREEN_HEIGHT;
            frame_line_pixels = frame_width;
        }
        else
        {
            frame_x = 0;
            frame_x_offset = (NES_SCREEN_WIDTH - w) / 2;
            frame_width = w;
            frame_height = NES_SCREEN_HEIGHT;
            frame_line_pixels = frame_width;
        }
        frame_y = (tft.height() - NES_SCREEN_HEIGHT) / 2;
    }
    else // assume 480x320
    {
        frame_x = 0;
        frame_y = 0;
        frame_x_offset = 8;
        frame_width = w;
        frame_height = h;
        frame_line_pixels = frame_width / 2;
    }
}
static unsigned int framecount = 0;
static void display_write_frame(const uint8_t *data[])
{
    xSemaphoreTake(menu_sync,portMAX_DELAY);
  // time critical
    static unsigned int chargecount = 0;
    tft.startWrite();
    if (w < 480)
    {
        tft.setAddrWindow(frame_x, frame_y, frame_width, frame_height);
        for (int32_t i = 0; i < NES_SCREEN_HEIGHT; i++)
        {
            uint8_t* p = (uint8_t *)(data[i] + frame_x_offset);
            for(int32_t j = 0;j<frame_line_pixels;j++) {
                tft.pushColor(myPalette[*p++]);
            }
        }
        if((framecount % 30)==0 && power.charging()) {
            draw_battery(300,4,(chargecount%101)/100.0f,false);
        } else if(framecount % 150 == 0) { 
            if(power.charged()) {
                draw_battery(300,4,1,false);
            } else {        
                draw_battery(300,4,power.level(),true);
            }
        }
        ++chargecount;
    }
    else
    {
        /* ST7796 480 x 320 resolution */

        /* Option 1:
         * crop 256 x 240 to 240 x 214
         * then scale up width x 2 and scale up height x 1.5
         * repeat a line for every 2 lines
         */
        // gfx->writeAddrWindow(frame_x, frame_y, frame_width, frame_height);
        // for (int16_t i = 10; i < (10 + 214); i++)
        // {
        //     gfx->writeIndexedPixelsDouble((uint8_t *)(data[i] + 8), myPalette, frame_line_pixels);
        //     if ((i % 2) == 1)
        //     {
        //         gfx->writeIndexedPixelsDouble((uint8_t *)(data[i] + 8), myPalette, frame_line_pixels);
        //     }
        // }

        /* Option 2:
         * crop 256 x 240 to 240 x 214
         * then scale up width x 2 and scale up height x 1.5
         * simply blank a line for every 2 lines
         */
        int16_t y = 0;
        for (int16_t i = 10; i < (10 + 214); i++)
        {
            tft.setAddrWindow(frame_x, y++, frame_width, 1);
            uint8_t* p = (uint8_t *)(data[i] + 8);
            for(int j = 0;j<frame_line_pixels;++j) {
                tft.pushBlock(*p++,2);
            }
            if ((i % 2) == 1)
            {
                y++; // blank line
            }
        }

        /* Option 3:
         * crop 256 x 240 to 240 x 240
         * then scale up width x 2 and scale up height x 1.33
         * repeat a line for every 3 lines
         */
        // gfx->writeAddrWindow(frame_x, frame_y, frame_width, frame_height);
        // for (int16_t i = 0; i < 240; i++)
        // {
        //     gfx->writeIndexedPixelsDouble((uint8_t *)(data[i] + 8), myPalette, frame_line_pixels);
        //     if ((i % 3) == 1)
        //     {
        //         gfx->writeIndexedPixelsDouble((uint8_t *)(data[i] + 8), myPalette, frame_line_pixels);
        //     }
        // }

        /* Option 4:
         * crop 256 x 240 to 240 x 240
         * then scale up width x 2 and scale up height x 1.33
         * simply blank a line for every 3 lines
         */
        // int16_t y = 0;
        // for (int16_t i = 0; i < 240; i++)
        // {
        //     gfx->writeAddrWindow(frame_x, y++, frame_width, 1);
        //     gfx->writeIndexedPixelsDouble((uint8_t *)(data[i] + 8), myPalette, frame_line_pixels);
        //     if ((i % 3) == 1)
        //     {
        //         y++; // blank line
        //     }
        // }
    }
    tft.endWrite();
    ++framecount;
    xSemaphoreGive(menu_sync);
}
static void display_clear()
{
    tft.fillScreen(bg_color);
}

//This runs on core 0.
QueueHandle_t vidQueue;
static void displayTask(void *arg)
{
	bitmap_t *bmp = NULL;
	while (1)
	{
		// xQueueReceive(vidQueue, &bmp, portMAX_DELAY); //skip one frame to drop to 30
		xQueueReceive(vidQueue, &bmp, portMAX_DELAY);
		display_write_frame((const uint8_t **)bmp->line);
	}
}

/* get info */
static char fb[1]; //dummy
bitmap_t *myBitmap;

/* initialise video */
static int init(int width, int height)
{
	return 0;
}

static void shutdown(void)
{
}

/* set a video mode */
static int set_mode(int width, int height)
{
	return 0;
}

/* copy nes palette over to hardware */
uint16 myPalette[256];
static void set_palette(rgb_t *pal)
{
	uint16 c;

	int i;

	for (i = 0; i < 256; i++)
	{
		c = (pal[i].b >> 3) + ((pal[i].g >> 2) << 5) + ((pal[i].r >> 3) << 11);
		//myPalette[i]=(c>>8)|((c&0xff)<<8);
		myPalette[i] = c;
	}
}

/* clear all frames to a particular color */
static void clear(uint8 color)
{
	// SDL_FillRect(mySurface, 0, color);
	display_clear();
}

/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{
	// SDL_LockSurface(mySurface);
	myBitmap = bmp_createhw((uint8 *)fb, NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT, NES_SCREEN_WIDTH * 2);
	return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
	bmp_destroy(&myBitmap);
}

static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects)
{
	xQueueSend(vidQueue, &bmp, 0);
	do_audio_frame();
}

viddriver_t sdlDriver =
	{
		"Simple DirectMedia Layer", /* name */
		init,						/* init */
		shutdown,					/* shutdown */
		set_mode,					/* set_mode */
		set_palette,				/* set_palette */
		clear,						/* clear */
		lock_write,					/* lock_write */
		free_write,					/* free_write */
		custom_blit,				/* custom_blit */
		false						/* invalidate flag */
};

extern "C" void osd_getvideoinfo(vidinfo_t *info)
{
	info->default_width = NES_SCREEN_WIDTH;
	info->default_height = NES_SCREEN_HEIGHT;
	info->driver = &sdlDriver;
}

/* input */
static void controller_init() {

}
static uint32_t controller_read_input() {
    gamepad_buttons b = input.read();
    if(b.menu) {
        start_menu();
    }
    return 0xFFFFFFFF ^ ((!b.down << 0) | (!b.up << 1) | (!b.right << 2) | (!b.left << 3) | (b.select << 4) | (b.start << 5) | (b.a << 6) | (b.b << 7) | (!b.l << 8) | (!b.r << 9));
}

static void osd_initinput()
{
	controller_init();
}

static void osd_freeinput(void)
{
}

void osd_getinput(void)
{
	const int ev[32] = {
		event_joypad1_up, event_joypad1_down, event_joypad1_left, event_joypad1_right,
		event_joypad1_select, event_joypad1_start, event_joypad1_a, event_joypad1_b,
		event_state_save, event_state_load, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0};
	static int oldb = 0xffff;
	uint32_t b = controller_read_input();
	uint32_t chg = b ^ oldb;
	int x;
	oldb = b;
	event_t evh;
	// nofrendo_log_printf("Input: %x\n", b);
	for (x = 0; x < 16; x++)
	{
		if (chg & 1)
		{
			evh = event_get(ev[x]);
			if (evh)
				evh((b & 1) ? INP_STATE_BREAK : INP_STATE_MAKE);
		}
		chg >>= 1;
		b >>= 1;
	}
}

void osd_getmouse(int *x, int *y, int *button)
{
}

/* init / shutdown */
static int logprint(const char *string)
{
	return printf("%s", string);
}

int osd_init()
{
    menu_sync = xSemaphoreCreateMutex();
	//nofrendo_log_chain_logfunc(logprint);
    setting_info::load(&settings);
	if (osd_init_sound())
		return -1;

	display_init();
	vidQueue = xQueueCreate(1, sizeof(bitmap_t *));
	// xTaskCreatePinnedToCore(&displayTask, "displayTask", 2048, NULL, 5, NULL, 1);
	xTaskCreatePinnedToCore(&displayTask, "displayTask", 2048, NULL, 0, NULL, 0);
	osd_initinput();
    state_setslot(settings.save_slot);
	return 0;
}

void osd_shutdown()
{
	osd_stopsound();
	osd_freeinput();
    vSemaphoreDelete(menu_sync);
}

char configfilename[] = "na";
extern "C" int osd_main(int argc, char *argv[])
{
	config.filename = configfilename;

	return main_loop(argv[0], system_autodetect);
}

//Seemingly, this will be called only once. Should call func with a freq of frequency,
extern "C" int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
	nofrendo_log_printf("Timer install, configTICK_RATE_HZ=%d, freq=%d\n", configTICK_RATE_HZ, frequency);
	timer = xTimerCreate("nes", configTICK_RATE_HZ / frequency, pdTRUE, NULL, (TimerCallbackFunction_t) func);
	xTimerStart(timer, 0);
	return 0;
}

/* filename manipulation */
extern "C" void osd_fullname(char *fullname, const char *shortname)
{
	strncpy(fullname, shortname, PATH_MAX);
}

/* This gives filenames for storage of saves */
extern "C" char *osd_newextension(char *string, char *ext)
{
	// dirty: assume both extensions is 3 characters
	size_t l = strlen(string);
	string[l - 3] = ext[1];
	string[l - 2] = ext[2];
	string[l - 1] = ext[3];

	return string;
}

/* This gives filenames for storage of PCX snapshots */
extern "C" int osd_makesnapname(char *filename, int len)
{
	return -1;
}
