/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "tftfunc.h"
#include "tft.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"


extern uint8_t test1_jpg_start[] asm("_binary_test1_jpg_start");
extern uint8_t test1_jpg_end[] asm("_binary_test1_jpg_end");
extern uint8_t test2_jpg_start[] asm("_binary_test2_jpg_start");
extern uint8_t test2_jpg_end[] asm("_binary_test2_jpg_end");
extern uint8_t test3_jpg_start[] asm("_binary_test3_jpg_start");
extern uint8_t test3_jpg_end[] asm("_binary_test3_jpg_end");
extern uint8_t test4_jpg_start[] asm("_binary_test4_jpg_start");
extern uint8_t test4_jpg_end[] asm("_binary_test4_jpg_end");

extern uint8_t tiger_bmp_start[] asm("_binary_tiger_bmp_start");
extern uint8_t tiger_bmp_end[] asm("_binary_tiger_bmp_end");

#define DISPLAY_READ 1

// ==================================================
// Define which spi bus to use VSPI_HOST or HSPI_HOST
#define SPI_BUS VSPI_HOST
// ==================================================

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/spiflash";

static uint8_t has_fs = 0;


#define DELAY 0x80


// We can use pre_cb callback to activate DC, but we are handling DC in low-level functions, so it is not necessary

// This function is called just before a transmission starts.
// It will set the D/C line to the value indicated in the user field.
/*
//-----------------------------------------------------------------------------
static void IRAM_ATTR ili_spi_pre_transfer_callback(spi_nodma_transaction_t *t) 
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}
*/

// Init for ILI7341
// ------------------------------------
static const uint8_t ILI9341_init[] = {
  24,                   					        // 24 commands in list
  ILI9341_SWRESET, DELAY,   						//  1: Software reset, no args, w/delay
  200,												//     200 ms delay
  ILI9341_POWERA, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  ILI9341_POWERB, 3, 0x00, 0XC1, 0X30,
  0xEF, 3, 0x03, 0x80, 0x02,
  ILI9341_DTCA, 3, 0x85, 0x00, 0x78,
  ILI9341_DTCB, 2, 0x00, 0x00,
  ILI9341_POWER_SEQ, 4, 0x64, 0x03, 0X12, 0X81,
  ILI9341_PRC, 1, 0x20,
  ILI9341_PWCTR1, 1, 0x23,							//Power control VRH[5:0]
  ILI9341_PWCTR2, 1, 0x10,							//Power control SAP[2:0];BT[3:0]
  ILI9341_VMCTR1, 2, 0x3e, 0x28,					//VCM control
  ILI9341_VMCTR2, 1, 0x86,							//VCM control2
  TFT_MADCTL, 1,									// Memory Access Control (orientation)
  (MADCTL_MV | MADCTL_BGR),
  // *** INTERFACE PIXEL FORMAT: 0x66 -> 18 bit; 0x55 -> 16 bit
  ILI9341_PIXFMT, 1, 0x55,
  TFT_INVOFF, 0,
  ILI9341_FRMCTR1, 2, 0x00, 0x18,
  ILI9341_DFUNCTR, 3, 0x08, 0x82, 0x27,				// Display Function Control
  TFT_PTLAR, 4, 0x00, 0x00, 0x01, 0x3F,
  ILI9341_3GAMMA_EN, 1, 0x00,						// 3Gamma Function Disable (0x02)
  ILI9341_GAMMASET, 1, 0x01,						//Gamma curve selected
  ILI9341_GMCTRP1, 15,   							//Positive Gamma Correction
  0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  ILI9341_GMCTRN1, 15,   							//Negative Gamma Correction
  0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  ILI9341_SLPOUT, DELAY, 							//  Sleep out
  120,			 									//  120 ms delay
  TFT_DISPON, 0,
};

// Init for ILI7341
// ------------------------------------
static const uint8_t ILI9488_init[] = {
  18,                   					        // 18 commands in list
  ILI9341_SWRESET, DELAY,   						//  1: Software reset, no args, w/delay
  200,												//     200 ms delay

  0xE0, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
  0xE1, 15,	0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,
  0xC0, 2,   //Power Control 1
	0x17,    //Vreg1out
	0x15,    //Verg2out

  0xC1, 1,   //Power Control 2
	0x41,    //VGH,VGL

  0xC5, 3,   //Power Control 3
	0x00,
	0x12,    //Vcom
	0x80,

  TFT_MADCTL, 1,									// Memory Access Control (orientation)
    (MADCTL_MV | MADCTL_BGR),

  // *** INTERFACE PIXEL FORMAT: 0x66 -> 18 bit; 0x55 -> 16 bit
  ILI9341_PIXFMT, 1, 0x66,

  0xB0, 1,   // Interface Mode Control
	0x00,    // 0x80: SDO NOT USE; 0x00 USE SDO

  0xB1, 1,   //Frame rate
	0xA0,    //60Hz

  0xB4, 1,   //Display Inversion Control
	0x02,    //2-dot

  0xB6, 2,   //Display Function Control  RGB/MCU Interface Control
	0x02,    //MCU
	0x02,    //Source,Gate scan direction

  0xE9, 1,   // Set Image Function
	0x00,    // Disable 24 bit data

  0x53, 1,   // Write CTRL Display Value
	0x28,    // BCTRL && DD on

  0x51, 1,   // Write Display Brightness Value
	0x7F,    //

  0xF7, 4,   // Adjust Control
	0xA9,
	0x51,
	0x2C,
	0x02,    // D7 stream, loose


  0x11, DELAY,  //Exit Sleep
    120,
  0x29, 0,      //Display on

};

//----------------------------------------------------
// Companion code to the above table. Reads and issues
// a series of LCD commands stored in byte array
//---------------------------------------------------------------------------
static void commandList(spi_nodma_device_handle_t spi, const uint8_t *addr) {
  uint8_t  numCommands, numArgs, cmd;
  uint16_t ms;

  numCommands = *addr++;         // Number of commands to follow
  while(numCommands--) {         // For each command...
    cmd = *addr++;               // save command
    numArgs  = *addr++;          //   Number of args to follow
    ms       = numArgs & DELAY;  //   If high bit set, delay follows args
    numArgs &= ~DELAY;           //   Mask out delay bit

	disp_spi_transfer_cmd_data(cmd, (uint8_t *)addr, numArgs);

	addr += numArgs;

    if(ms) {
      ms = *addr++;              // Read post-command delay time (ms)
      if(ms == 255) ms = 500;    // If 255, delay for 500 ms
	  vTaskDelay(ms / portTICK_RATE_MS);
    }
  }
}

//Initialize the display
//-------------------------------------------------
static void ili_init(spi_nodma_device_handle_t spi) 
{
    esp_err_t ret;
    uint8_t tft_pix_fmt = DISP_COLOR_BITS_24;

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);

#if PIN_NUM_BCKL
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_OFF);
#endif

#if PIN_NUM_RST
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
#endif

    //Send all the initialization commands
	if (tft_disp_type == DISP_TYPE_ILI9341) {
		_width = ILI9341_HEIGHT;
		_height =ILI9341_WIDTH;
		ret = disp_select();
		assert(ret==ESP_OK);
		commandList(spi, ILI9341_init);
	}
	else if (tft_disp_type == DISP_TYPE_ILI9488) {
		COLOR_BITS = 24;  // only 18-bit color format supported
		_width = ILI9488_HEIGHT;
		_height = ILI9488_WIDTH;
		ret = disp_select();
		assert(ret==ESP_OK);
		commandList(spi, ILI9488_init);
	}
	else assert(0);

    if ((COLOR_BITS == 16) && (tft_disp_type == DISP_TYPE_ILI9341)) tft_pix_fmt = DISP_COLOR_BITS_16;
    disp_spi_transfer_cmd_data(ILI9341_PIXFMT, &tft_pix_fmt, 1);

    ret = disp_deselect();
	assert(ret==ESP_OK);

	///Enable backlight
#if PIN_NUM_BCKL
    gpio_set_level(PIN_NUM_BCKL, PIN_BCKL_ON);
#endif
}


//Simple routine to test basic display functions and speed at different spi clocks
//--------------------------------------------------------------------------------------
static void display_test(spi_nodma_device_handle_t spi, spi_nodma_device_handle_t tsspi) 
{
	int max_speed = 4;
    uint32_t speeds[5] = {8000000,10000000,16000000,30000000, 40000000};

    int speed_idx = 0;
    uint32_t change_speed;
	esp_err_t ret;
    uint8_t *line_buf;
    uint8_t *line_rdbuf;
    int x, y, ry;
	int line_check=0;
	color_t color;
    uint32_t t1=0,t2=0,t3=0, tstart;
	float hue_inc;
#if USE_TOUCH
	int tx, ty;
#endif

	ry = rand() % (_height-1);
	ry = rand() % (_height-1);
	ry = rand() % (_height-1);

	line_buf = malloc(_width*sizeof(color_t)+1);
	assert(line_buf);
	line_rdbuf = malloc(_width*sizeof(color_t)+1);
	assert(line_rdbuf);
	color_t *line = (color_t *)(line_buf+1);
	color_t *rdline = (color_t *)(line_rdbuf+1);

	printf("Display: %s (%d,%d)\r\n\r\n", ((tft_disp_type == DISP_TYPE_ILI9341) ? "ILI9341" : "ILI9488"), _width, _height);

	while(1) {
		// ===== Clear screen =====
		color = TFT_BLACK;
		ret = disp_select();
		assert(ret==ESP_OK);

		TFT_pushColorRep(0, 0, _width-1, _height-1, color, _width*_height);

		color = TFT_WHITE;
		for (y=0;y<_height;y+=20) {
			TFT_pushColorRep(0, y, _width-1, y, color, _width);
		}
		for (x=0;x<_width;x+=20) {
			TFT_pushColorRep(x, 0, x, _height-1, color,  _height);
		}
		TFT_pushColorRep(0, _height-1, _width-1, _height-1, color,  _width);
		TFT_pushColorRep(_width-1, 0, _width-1, _height-1, color,  _height);

		ret = disp_deselect();
		assert(ret==ESP_OK);
		vTaskDelay(1000 / portTICK_RATE_MS);

		// ===== Send color lines =====
		ry = rand() % (_height-1);
		tstart = clock();
		ret = disp_select();
		assert(ret==ESP_OK);
		for (y=0; y<_height; y++) {
			hue_inc = (float)(((float)y / (float)(_height-1) * 360.0));
            for (x=0; x<_width; x++) {
				color = HSBtoRGB(hue_inc, 1.0, (float)x / (float)_width);
				line[x] = color;
			}
#if DISPLAY_READ
            // save line for read compare
            if (y == ry) memcpy(line_rdbuf, line_buf, _width * sizeof(color_t) + 1);
#endif
    	    send_data(0, y, _width-1, y, _width, line);
		}
		t1 = clock() - tstart;
		ret = disp_deselect();
		assert(ret==ESP_OK);

		line_check=0;
#if DISPLAY_READ
		// == Check line ==

		// Read color line
		ret = read_data(0, ry, _width-1, ry, _width, line_buf);

		if (ret == ESP_OK) {
			for (y=0; y<_width; y++) {
				if (compare_colors(line[y], rdline[y])) {
					//printf("{%02x,%02x,%02x} <> {%02x,%02x,%02x}\r\n", line[y].r, line[y].g, line[y].b, rdline[y].r, rdline[y].g, rdline[y].b);
					line_check = y+1;
					break;
				}
			}
        }

#endif
		vTaskDelay(1000 / portTICK_RATE_MS);

		// ===== Display pixels =====
		uint8_t clidx = 0;
		color = TFT_RED;
		tstart = clock();
		ret = disp_select();
		assert(ret==ESP_OK);

		for (y=0; y<_height; y++) {
			for (x=0; x<_width; x++) {
				drawPixel(x, y, color, 0);
			}
			if ((y > 0) && ((y % 40) == 0)) {
				clidx++;
				if (clidx > 5) clidx = 0;

				if (clidx == 0) color = TFT_RED;
				else if (clidx == 1) color = TFT_GREEN;
				else if (clidx == 2) color = TFT_BLUE;
				else if (clidx == 3) color = TFT_YELLOW;
				else if (clidx == 4) color = TFT_CYAN;
				else color = TFT_PURPLE;
			}
		}
		ret = disp_deselect();
		assert(ret==ESP_OK);
		t2 = clock() - tstart;
		vTaskDelay(1000 / portTICK_RATE_MS);
		
		// ===== Clear screen with color =====
		color = TFT_BLUE;
		tstart = clock();
		TFT_pushColorRep(0, 0, _width-1, _height-1, color, _width*_height);
		t3 = clock() - tstart;
		vTaskDelay(1000 / portTICK_RATE_MS);

		// *** Print info
		printf("-------------\r\n");
		printf(" Disp clock = %5.2f MHz (requested: %5.2f)\r\n", (float)((float)(spi_nodma_get_speed(spi))/1000000.0), (float)(speeds[speed_idx])/1000000.0);
		printf("      Lines = %5d  ms (%d lines of %d pixels)\r\n",t1,_height,_width);
#if DISPLAY_READ
		printf(" Read check   ");
		if (line_check == 0) printf("   OK, line %d", ry);
		else printf("  Err, on line %d at %d", ry, line_check);
		if (speeds[speed_idx] > max_rdclock) {
			printf(" (Read clock = %5.2f MHz)", (float)(max_rdclock/1000000.0));
		}
		printf("\r\n");
#endif
		printf("     Pixels = %5d  ms (%dx%d)\r\n",t2,_width,_height);
		printf("        Cls = %5d  ms (%dx%d)\r\n",t3,_width,_height);

#if USE_TOUCH
		if ( tft_read_touch(&tx, &ty, 1)) {
			printf(" Touched at (%d,%d) [row TS values]\r\n",tx,ty);
		}
#endif
		printf("-------------\r\n");

		// Change SPI speed
		speed_idx++;
		if (speed_idx > max_speed) break;

		change_speed = spi_nodma_set_speed(spi, speeds[speed_idx]);
		assert(change_speed > 0 );
    }
	free(line_rdbuf);
	free(line_buf);
}

//-----------------------------
static color_t random_color() {

	color_t color;
	color.r  = (uint8_t)(rand() % 255);
	color.g  = (uint8_t)(rand() % 255);
	color.b  = (uint8_t)(rand() % 255);
	return color;
}

static int _demo_pass = 0;

//------------------
void disp_images() {
	uint32_t tstart;
	uint8_t scale = ((_width > 400) ? 0 : 1);

	// Change DMA mode on every pass
	tft_use_trans = ((_demo_pass & 1) ? 1 : 0);
	// Change gray scale mode on every 2nd pass
	gray_scale = ((_demo_pass & 2) ? 1 : 0);

	if (_demo_pass < 4) printf("\r\n         DMA mode: %s\r\n", ((tft_use_trans) ? "ON" : "OFF"));
	if (_demo_pass < 4) printf("   GRAYSCALE mode: %s\r\n", ((gray_scale) ? "ON" : "OFF"));
	if (_demo_pass < 4) printf("          Display: %d,%d jpg scale: %d Color mode: %d-bit\r\n\r\n", _width, _height, scale, COLOR_BITS);

	if (_demo_pass < 4) {
		// ** Show Cls and send_line timings
		tstart = clock();
		TFT_fillScreen(TFT_BLUE);
		tstart = clock() - tstart;
		printf("Clear screen time: %u ms\r\n", tstart);
		vTaskDelay(1000 / portTICK_RATE_MS);

		color_t color;
		float hue_inc = (float)((10.0 / (float)(_height-1) * 360.0));
		for (int x=0; x<_width; x++) {
			color = HSBtoRGB(hue_inc, 1.0, (float)x / (float)_width);
			tft_line[x] = color;
		}

		tstart = clock();
		for (int n=0; n<1000; n++) {
			send_data(0, _height/2+(n&3), _width-1, _height/2+(n&3), (uint32_t)_width, tft_line);
		}
		tstart = clock() - tstart;
		printf("Display line time: %u us\r\n", tstart);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	TFT_fillScreen(TFT_BLACK);

	// ** Show scaled (half size) JPG images
	tft_jpg_image(0, 0, scale+1, NULL, test1_jpg_start, test1_jpg_end-test1_jpg_start);
	vTaskDelay(500 / portTICK_RATE_MS);
	tft_jpg_image(_width/2, 0, scale+1, NULL, test2_jpg_start, test2_jpg_end-test2_jpg_start);
	vTaskDelay(500 / portTICK_RATE_MS);
	tft_jpg_image(0, _height/2, scale+1, NULL, test3_jpg_start, test3_jpg_end-test3_jpg_start);
	vTaskDelay(500 / portTICK_RATE_MS);
	tft_jpg_image(_width/2, _height/2, scale+1, NULL, test4_jpg_start, test4_jpg_end-test4_jpg_start);
	vTaskDelay(5000 / portTICK_RATE_MS);

	TFT_fillScreen(TFT_BLACK);

	// ** Show full size JPG images
	tstart = clock();
	tft_jpg_image(0, 0, scale, NULL, test1_jpg_start, test1_jpg_end-test1_jpg_start);
	tstart = clock() - tstart;
	if (_demo_pass < 4) printf("  JPG Decode time: %u ms\r\n", tstart);
	vTaskDelay(1000 / portTICK_RATE_MS);

	if (_demo_pass < 4) printf("Decode JPG (file): ");
	if (has_fs) {
		struct stat sb;
		if ((stat("/spiflash/image.jpg", &sb) == 0) && (sb.st_size > 0)) {
		    FILE *f = fopen("/spiflash/image.jpg", "rb");
		    if (f) {
				tstart = clock();
				tft_jpg_image(0, 0, scale, NULL, test1_jpg_start, test1_jpg_end-test1_jpg_start);
				tstart = clock() - tstart;
				fclose(f);
				if (_demo_pass < 4) printf("%u ms\r\n", tstart);
				vTaskDelay(1000 / portTICK_RATE_MS);
		    }
			else if (_demo_pass < 4) printf("Error opening file\r\n");
		}
		else if (_demo_pass < 4) printf("File not found\r\n");
	}
	else if (_demo_pass < 4) printf("No file system found\r\n");

	tft_jpg_image(0, 0, scale, NULL, test2_jpg_start, test2_jpg_end-test2_jpg_start);
	vTaskDelay(2000 / portTICK_RATE_MS);
	tft_jpg_image(0, 0, scale, NULL, test3_jpg_start, test3_jpg_end-test3_jpg_start);
	vTaskDelay(2000 / portTICK_RATE_MS);
	tft_jpg_image(0, 0, 0, NULL, test4_jpg_start, test4_jpg_end-test4_jpg_start);

	// Print transparent text on last image
	_transparent = 1;
	_fg = TFT_BLACK;
	TFT_setFont(TOONEY32_FONT, NULL);

	int swidth = getStringWidth("JPG IMAGE");
	TFT_print("JPG IMAGE", ((_width - swidth - 1) / 2), _height-40);
	_fg = TFT_CYAN;
	TFT_print("JPG IMAGE", ((_width - swidth - 1) / 2), _height-38);
	vTaskDelay(5000 / portTICK_RATE_MS);

	// ** Show BMP image
	tstart = clock();
	tft_bmp_image(0, 0, NULL, tiger_bmp_start, tiger_bmp_end-tiger_bmp_start);
	tstart = clock() - tstart;
	if (_demo_pass < 4) printf("  BMP Decode time: %u ms\r\n", tstart);
	vTaskDelay(1000 / portTICK_RATE_MS);

	// Print transparent text on image
	_fg = TFT_BLACK;
	swidth = getStringWidth("BMP IMAGE");
	TFT_print("BMP IMAGE", ((_width - swidth - 1) / 2), _height-40);
	_fg = TFT_CYAN;
	TFT_print("BMP IMAGE", ((_width - swidth - 1) / 2), _height-38);
	vTaskDelay(4000 / portTICK_RATE_MS);
	_transparent = 0;

	_demo_pass++;
}

//-----------------
void disp_frame() {
	TFT_fillScreen(TFT_BLACK);

	TFT_drawRect(0, 0, _width-1, _height-1, TFT_YELLOW);
	TFT_drawRect(2, 2, _width-5, _height-5, TFT_BLUE);
}

//----------------
void disp_text() {
	_fg = random_color();
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_print("Hi from ESP32 spi_master_nodma driver", 70, 6);

	_fg = random_color();
	TFT_setFont(DEFAULT_FONT, NULL);
	rotation = 270;
	TFT_print("Rotated text 270'", 6, _height-70);
	rotation = 90;
	_fg = random_color();
	TFT_print("Rotated text 90'", _width-tft_getfontheight(), 60);
	rotation = 0;

	TFT_setFont(TOONEY32_FONT, NULL);
	int fsz = tft_getfontheight();
	_fg = random_color();
	TFT_print("Welcome ESP32", CENTER, CENTER);

	_fg = random_color();
	TFT_setFont(COMIC24_FONT, NULL);
	TFT_print(" * ESP32 *", CENTER, (_height/2 - 4 - tft_getfontheight()));

	_fg = random_color();
	TFT_setFont(DEJAVU18_FONT, NULL);
	TFT_print("2.4 GHz Wi-Fi & Bluetooth", CENTER, _height/2 + fsz);

	TFT_setFont(FONT_7SEG, NULL);
	if (gray_scale) _fg = TFT_YELLOW;
	else _fg = TFT_BLUE;

	if (_width > 320) {
		if (gray_scale) {
			_fg = TFT_YELLOW;
			set_font_atrib(14, 3, 1, (color_t){192,192,0});
		}
		else {
			_fg = TFT_BLUE;
			set_font_atrib(14, 3, 1, (color_t){0,0,192});
		}
	}
	else {
		if (gray_scale) {
			_fg = TFT_YELLOW;
			set_font_atrib(10, 2, 1, (color_t){192,192,0});
		}
		else {
			_fg = TFT_BLUE;
			set_font_atrib(10, 2, 1, (color_t){0,0,192});
		}
	}
}

//===============
void tft_test() {
	TFT_setRotation(LANDSCAPE);
	tft_resetclipwin();

#if USE_TOUCH
	int tx, ty;
#endif
	rotation = 0;
	_wrap = 0;
	_transparent = 0;
	_forceFixed = 0;
	tft_resetclipwin();

	color_t color = TFT_BLACK, fill = TFT_BLACK;
	struct tm* tm_info;
    char buffer[64];
	time_t now;
	time(&now);
	tm_info = localtime(&now);
	int nsec = -1;
	uint64_t nn = 0;
	uint8_t xdir = 0;
	int cx = ((_width > 320) ? 50 : 32);
	int cr = ((_width > 320) ? 44 : 16);
	uint16_t rpos = cx*2;

	tft_use_trans = 0;
	gray_scale = 0;

	while (1) {
		time(&now);
		tm_info = localtime(&now);

		if (clock() > nn ) {
			disp_images();
			disp_frame();
			disp_text();
			nn = clock() + 30000;
		}

		if ((rpos % 50) == 0) color = random_color();
		TFT_fillRect(rpos, 34, 16, 16, TFT_BLACK);
		if (xdir == 0) {
			rpos++;
			if (rpos > (_width - (cx*2))) {
				rpos -= 2;
				xdir = 1;
			}
		}
		else {
			rpos--;
			if (rpos < (cx*2)) {
				rpos += 2;
				xdir = 0;
			}
		}
		TFT_fillRect(rpos, 34, 16, 16, color);

		if (tm_info->tm_sec != nsec) {
			nsec = tm_info->tm_sec;

			color = random_color();
			fill = random_color();
			TFT_fillCircle(cx,cx,cr, fill);
			TFT_drawCircle(cx,cx,cr, color);

			color = random_color();
			fill = random_color();
			TFT_fillCircle(cx,_height-cx,cr, fill);
			TFT_drawCircle(cx,_height-cx,cr, color);

			color = random_color();
			fill = random_color();
			TFT_fillCircle(_width-cx,cx,cr, fill);
			TFT_drawCircle(_width-cx,cx,cr, color);

			color = random_color();
			fill = random_color();
			TFT_fillCircle(_width-cx,_height-cx,cr, fill);
			TFT_drawCircle(_width-cx,_height-cx,cr, color);

			disp_text();

			strftime(buffer, 16, "%H:%M:%S", tm_info);
			TFT_print(buffer, CENTER, _height - 10 - tft_getfontheight());
#if USE_TOUCH
			// Get touch status
			_fg = TFT_YELLOW;
			TFT_setFont(DEFAULT_FONT, NULL);
			if ( tft_read_touch(&tx, &ty, 0)) {
				// Touched
				sprintf(buffer, "TOUCH at %d,%d", tx,ty);
				_fg = TFT_YELLOW;
				TFT_setFont(DEFAULT_FONT, NULL);
				TFT_print(buffer, CENTER, 58);
			}
			else TFT_fillRect(50, 58, _width-50, tft_getfontheight(), TFT_BLACK);
#endif
		}

    	vTaskDelay(20 / portTICK_RATE_MS);
	}
}

//---------------------------
static esp_err_t initfs(void)
{
	const char *TAG = "[SPI_FS]";

	ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
        return 1;
    }
    ESP_LOGE(TAG, "FATFS mounted");

    has_fs = 1;

	struct stat sb;
	if ((stat("/spiflash/image.jpg", &sb) == 0) && (sb.st_size > 0)) {
	    ESP_LOGE(TAG, "JPG image found on file system");
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Saving jpg file");
    FILE *f = fopen("/spiflash/image.jpg", "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return 2;
    }

    int idx = 0;
    int len = 0;
    int size = test1_jpg_end-test1_jpg_start;
    while (idx < size) {
    	len = 1024;
    	if ((idx + 1024) > size) len = size - idx;
    	else len = 1024;
    	int wrlen = fwrite(test1_jpg_start + idx, 1, len, f);
    	if (wrlen != len) {
    		fclose(f);
            ESP_LOGE(TAG, "Error writing to file");
    		return 3;
    	}
    	idx += wrlen;
    }

    fclose(f);
    ESP_LOGI(TAG, "JPG image saved to file system");
    return ESP_OK;

    // Unmount FATFS
    //ESP_LOGI(TAG, "Unmounting FAT filesystem");
    //ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));

    //ESP_LOGI(TAG, "Done");
}


//-------------
void app_main()
{
    esp_err_t ret;

    // ==== Set display type =============================
	tft_disp_type = DISP_TYPE_ILI9341; //DISP_TYPE_ILI9488
    // ===================================================

	// ==== Set color format =============================
	// ==== COLOR_BITS = 24 valid only for ILI9341 =======
	COLOR_BITS = 16;
	// ===================================================

	// ==== Set maximum spi clock for display read operations ====
	max_rdclock = 16000000;
	// ===========================================================

    // ** Initialize the file system
	ret = initfs();
    assert(ret==ESP_OK);

    // ** Allocate global tft line buffer
    tft_line = malloc((TFT_LINEBUF_MAX_SIZE*3) + 1);
    assert(tft_line);

    // Configure SPI device(s) used
	gpio_set_direction(PIN_NUM_MISO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_NUM_MISO, GPIO_PULLUP_ONLY);

    spi_nodma_device_handle_t spi;
    spi_nodma_device_handle_t tsspi = NULL;
	
    spi_nodma_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };
    spi_nodma_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                //Initial clock out at 8 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=-1,                       //we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           //external CS pin
		.flags=SPI_DEVICE_HALFDUPLEX,           //Set half duplex mode (Full duplex mode can also be set by commenting this line
												// but we don't need full duplex in  this example
												// also, SOME OF TFT FUNCTIONS ONLY WORKS IN HALF DUPLEX MODE
		.queue_size = 2,						// in some tft functions we are using DMA mode, so we need queues!
		// We can use pre_cb callback to activate DC, but we are handling DC in low-level functions, so it is not necessary
        //.pre_cb=ili_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };

#if USE_TOUCH
	spi_nodma_device_interface_config_t tsdevcfg={
        .clock_speed_hz=2500000,                //Clock out at 2.5 MHz
        .mode=0,                                //SPI mode 2
        .spics_io_num=PIN_NUM_TCS,              //Touch CS pin
		.spics_ext_io_num=-1,                   //Not using the external CS
		.command_bits=8,                        //1 byte command
		.queue_size = 1,
    };
#endif

	vTaskDelay(500 / portTICK_RATE_MS);
	printf("\r\n===================================\r\n");
	printf("spi_master_nodma demo, LoBo 05/2017\r\n");
	printf("===================================\r\n\r\n");

	//Initialize the SPI bus and attach the LCD to the SPI bus
    ret=spi_nodma_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus\r\n");
	disp_spi = spi;

	// Test select/deselect
	ret = spi_nodma_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_nodma_device_deselect(spi);
    assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", spi_nodma_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_nodma_uses_native_pins(spi) ? "true" : "false");

#if USE_TOUCH
    //Attach the touch screen to the same SPI bus
    ret=spi_nodma_bus_add_device(SPI_BUS, &buscfg, &tsdevcfg, &tsspi);
    assert(ret==ESP_OK);
	printf("SPI: touch screen device added to spi bus\r\n");
	ts_spi = tsspi;

	// Test select/deselect
	ret = spi_nodma_device_select(tsspi, 1);
    assert(ret==ESP_OK);
	ret = spi_nodma_device_deselect(tsspi);
    assert(ret==ESP_OK);

	printf("SPI: attached TS device, speed=%u\r\n", spi_nodma_get_speed(tsspi));
#endif

	//Initialize the LCD
	printf("SPI: display init...\r\n");
    ili_init(spi);
	printf("OK\r\n");
	vTaskDelay(500 / portTICK_RATE_MS);
	
	display_test(spi, tsspi);

	spi_nodma_set_speed(spi, 40000000);

	printf("\r\nGraphics demo started\r\n");
	printf("---------------------\r\n");
	tft_test();
}
