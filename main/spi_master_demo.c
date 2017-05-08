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

// Handle of the wear levelling library instance
//static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/spiflash";


/*
 This code tests accessing ILI9341 based display and prints some timings

 Some fancy graphics is displayed on the ILI9341-based 320x240 LCD

 Reading the display content is demonstrated.
 
 If Touch screen is available, reading the touch coordinates (non calibrated) is also demonstrated.
*/


#define DISPLAY_READ 1

// define which spi bus to use VSPI_HOST or HSPI_HOST
#define SPI_BUS VSPI_HOST

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
  23,                   					        // 23 commands in list
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
  ILI9341_PIXFMT, 1, 0x55,
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

  // *** INTERFACE PIXEL FORMAT: 0x66 -> 18 bit; 0x55 -> 16 bit; 0x11 -> 1 bit
#if (COLOR_BITS ==24)
  ILI9341_PIXFMT, 1, 0x66,
#else
  ILI9341_PIXFMT, 1, 0x55,
#endif

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

	disp_spi_transfer_cmd_data(spi, cmd, (uint8_t *)addr, numArgs);

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
    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
	/* Reset and backlit pins are not used
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
	*/

    //Send all the commands
#if DISP_TYPE_ILI9341
    _width = ILI9341_HEIGHT;
    _height =ILI9341_WIDTH;
	ret = spi_nodma_device_select(spi, 0);
	assert(ret==ESP_OK);
    commandList(spi, ILI9341_init);
#elif DISP_TYPE_ILI9488
    _width  = ILI9488_HEIGHT;
    _height = ILI9488_WIDTH;
	ret = spi_nodma_device_select(spi, 0);
	assert(ret==ESP_OK);
    commandList(spi, ILI9488_init);
#else
    	assert(0);
#endif

    ret = spi_nodma_device_deselect(spi);
	assert(ret==ESP_OK);

    ///Enable backlight
    //gpio_set_level(PIN_NUM_BCKL, 0);
}


#if USE_TOUCH
//Send a command to the Touch screen
//---------------------------------------------------------------
uint16_t ts_cmd(spi_nodma_device_handle_t spi, const uint8_t cmd) 
{
    esp_err_t ret;
	spi_nodma_transaction_t t;
	uint8_t rxdata[2] = {0};

	// send command byte & receive 2 byte response
	memset(&t, 0, sizeof(t));            //Zero out the transaction
    t.rxlength=8*2;
    t.rx_buffer=&rxdata;
	t.command = cmd;

	ret = spi_nodma_transfer_data(spi, &t);    // Transmit using direct mode
    assert(ret==ESP_OK);                 //Should have had no issues.
	//printf("TS: %02x,%02x\r\n",rxdata[0],rxdata[1]);

	return (((uint16_t)(rxdata[0] << 8) | (uint16_t)(rxdata[1])) >> 4);
}
#endif


//Simple routine to test display functions in DMA/transactions & direct mode
//--------------------------------------------------------------------------------------
static void display_test(spi_nodma_device_handle_t spi, spi_nodma_device_handle_t tsspi) 
{
	int max_speed = 4;
    uint32_t speeds[5] = {2500000,5000000,8000000,16000000, 40000000};

    int speed_idx = 0, max_rdspeed=5000000;
    uint32_t change_speed, rd_clk;
	esp_err_t ret;
    uint8_t *line_buf;
    uint8_t *line_rdbuf;
    int x, y, ry;
	int line_check=0;
	color_t color;
    uint32_t t1=0,t2=0,t3=0, tstart;
	float hue_inc;
#if USE_TOUCH
	int tx, ty, tz=0;
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

	while(1) {
		// ===== Clear screen =====
		color = TFT_BLACK;
		ret = spi_nodma_device_select(spi, 0);
		assert(ret==ESP_OK);

		disp_spi_transfer_addrwin(spi, 0, _width-1, 0, _height-1);
		disp_spi_transfer_color_rep(spi, &color, _width*_height, 1);

		color = TFT_WHITE;
		for (y=0;y<_height;y+=20) {
			disp_spi_transfer_addrwin(spi, 0, _width-1, y, y);
			disp_spi_transfer_color_rep(spi, &color, _width, 1);
		}
		for (x=0;x<_width;x+=20) {
			disp_spi_transfer_addrwin(spi, x, x, 0, _height-1);
			disp_spi_transfer_color_rep(spi, &color,  _height, 1);
		}
		disp_spi_transfer_addrwin(spi, 0, _width-1, _height-1, _height-1);
		disp_spi_transfer_color_rep(spi, &color,  _width, 1);
		disp_spi_transfer_addrwin(spi, _width-1, _width-1, 0, _height-1);
		disp_spi_transfer_color_rep(spi, &color,  _height, 1);

		ret = spi_nodma_device_deselect(spi);
		assert(ret==ESP_OK);
		vTaskDelay(1000 / portTICK_RATE_MS);

		// ===== Send color lines =====
		ry = rand() % (_height-1);
		tstart = clock();
		ret = spi_nodma_device_select(spi, 0);
		assert(ret==ESP_OK);
		line_check=0;
		for (y=0; y<_height; y++) {
			hue_inc = (float)(((float)y / (float)(_height-1) * 360.0));
            for (x=0; x<_width; x++) {
				color = HSBtoRGB(hue_inc, 1.0, (float)x / (float)_width);
				line[x] = color;
			}
			disp_spi_transfer_addrwin(spi, 0, _width-1, y, y);
			disp_spi_transfer_color_rep(spi, line,  _width, 0);
#if DISPLAY_READ
            if (y == ry) memcpy(line_rdbuf, line_buf, _width * sizeof(color_t) + 1);
#endif
		}
		t1 = clock() - tstart;

#if DISPLAY_READ
		// == Check line ==
		if (speed_idx > max_rdspeed) {
			// Set speed to max read speed
			change_speed = spi_nodma_set_speed(spi, speeds[max_rdspeed]);
			assert(change_speed > 0 );
			ret = spi_nodma_device_select(spi, 0);
			assert(ret==ESP_OK);
			rd_clk = speeds[max_rdspeed];
		}
		else rd_clk = speeds[speed_idx];
		ret = read_data(0, ry, _width-1, ry, _width, line_buf, 0);
		if (ret == ESP_OK) {
            //line_check = memcmp((uint8_t *)(line[0]), (uint8_t *)(line[1]), _width*2);
			for (y=0; y<_width; y++) {
				if (compare_colors(line[y], rdline[y])) {
					line_check = y+1;
					break;
				}
			}
			if (line_check) {
				for (y=1;y < 37;y+=3) {
					printf("(%02x,%02x,%02x) ", line_buf[y], line_buf[y+1], line_buf[y+2]);
				}
				printf("\r\n");
				for (y=1;y < 37;y+=3) {
					printf("(%02x,%02x,%02x) ", line_rdbuf[y], line_rdbuf[y+1], line_rdbuf[y+2]);
				}
				printf("\r\n");
				for (x=0;x<13;x++) {
					color = readPixel(x, ry, 0);
					printf("(%02x,%02x,%02x) ", color.r, color.g, color.b);
				}
				printf("\r\n");
			}
        }
		if (speed_idx > max_rdspeed) {
			// Restore spi speed
			change_speed = spi_nodma_set_speed(spi, speeds[speed_idx]);
			assert(change_speed > 0 );
		}

		ret =spi_nodma_device_deselect(spi);
		assert(ret==ESP_OK);
		if (line_check) {
			// Error checking line
			if (max_rdspeed > speed_idx) {
				// speed probably too high for read
				if (speed_idx) max_rdspeed = speed_idx - 1;
				else max_rdspeed = 0;
				printf("### MAX READ SPI CLOCK = %d ###\r\n", speeds[max_rdspeed]);
			}
			else {
				// Set new max spi clock, reinitialize the display
				if (speed_idx) max_speed = speed_idx - 1;
				else max_speed = 0;
				printf("### MAX SPI CLOCK = %d ###\r\n", speeds[max_speed]);
				speed_idx = 0;
				spi_nodma_set_speed(spi, speeds[speed_idx]);
				if (speed_idx) {
					ili_init(spi);
					continue;
				}
			}
        }
#endif
		vTaskDelay(1000 / portTICK_RATE_MS);

		// ===== Display pixels =====
		uint8_t clidx = 0;
		color = TFT_RED;
		tstart = clock();
		ret = spi_nodma_device_select(spi, 0);
		assert(ret==ESP_OK);
		for (y=0; y<_height; y++) {
			for (x=0; x<_width; x++) {
				disp_spi_set_pixel(spi, x, y, color);
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
		ret = spi_nodma_device_deselect(spi);
		assert(ret==ESP_OK);
		t2 = clock() - tstart;
		vTaskDelay(1000 / portTICK_RATE_MS);
		
		// ===== Clear screen with color =====
		color = TFT_BLUE;
		tstart = clock();
		TFT_pushColorRep(0, _width-1, 0, _height-1, color, _width*_height);
		t3 = clock() - tstart;
		vTaskDelay(1000 / portTICK_RATE_MS);

#if USE_TOUCH
		// Get toush status
		//ret = spi_nodma_device_deselect(tsspi);
		//assert(ret==ESP_OK);
		tz = ts_cmd(tsspi, 0xB0);
		if (tz > 100) {
			// Touched, get coordinates
			tx = ts_cmd(tsspi, 0xD0);
			ty = ts_cmd(tsspi, 0x90);
		}
		//ret = spi_nodma_device_deselect(tsspi);
		//assert(ret==ESP_OK);
#endif

		// *** Print info
		printf("-------------\r\n");
		printf(" Disp clock = %5.2f MHz (requested: %5.2f)\r\n", (float)((float)(spi_nodma_get_speed(spi))/1000000.0), (float)(speeds[speed_idx])/1000000.0);
		printf("      Lines = %5d  ms (%d lines of %d pixels)\r\n",t1,_height,_width);
#if DISPLAY_READ
		printf(" Read check   ");
		if (line_check == 0) printf("   OK, line %d", ry);
		else printf("  Err, on line %d at %d", ry, line_check);
		if (speed_idx > max_rdspeed) {
			printf(" (Read clock = %5.2f MHz)", (float)(rd_clk/1000000.0));
		}
		printf("\r\n");
#endif
		printf("     Pixels = %5d  ms (%dx%d)\r\n",t2,_width,_height);
		printf("        Cls = %5d  ms (%dx%d)\r\n",t3,_width,_height);
#if USE_TOUCH
		if (tz > 100) {
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

static int _img_pass = 0;

//------------------
void disp_images() {
	uint32_t tstart;
/*
	tstart = clock();
	disp_spi_transfer_color_rep_trans(0, 0, _width-1, _height-1, TFT_BLUE, (uint32_t)(_height*_width));
	tstart = clock() - tstart;
	if (_img_pass == 0) printf("Clear screen time (TRANS): %u ms\r\n", tstart);
	vTaskDelay(500 / portTICK_RATE_MS);
*/
	tstart = clock();
	TFT_fillScreen(TFT_BLACK);
	tstart = clock() - tstart;
	if (_img_pass == 0) printf("Clear screen time: %u ms\r\n", tstart);

	tft_jpg_image(0, 0, 1, NULL, test1_jpg_start, test1_jpg_end-test1_jpg_start);
	vTaskDelay(500 / portTICK_RATE_MS);
	tft_jpg_image(_width/2, 0, 1, NULL, test2_jpg_start, test2_jpg_end-test2_jpg_start);
	vTaskDelay(500 / portTICK_RATE_MS);
	tft_jpg_image(0, _height/2, 1, NULL, test3_jpg_start, test3_jpg_end-test3_jpg_start);
	vTaskDelay(500 / portTICK_RATE_MS);
	tft_jpg_image(_width/2, _height/2, 1, NULL, test4_jpg_start, test4_jpg_end-test4_jpg_start);
	vTaskDelay(5000 / portTICK_RATE_MS);

	tft_use_trans = 0;
	tstart = clock();
	tft_jpg_image(0, 0, 0, NULL, test1_jpg_start, test1_jpg_end-test1_jpg_start);
	tstart = clock() - tstart;
	if (_img_pass == 0) printf("JPG Decode time (no trans): %u ms\r\n", tstart);
	vTaskDelay(1000 / portTICK_RATE_MS);

	tft_use_trans = 1;
	tstart = clock();
	tft_jpg_image(0, 0, 0, NULL, test1_jpg_start, test1_jpg_end-test1_jpg_start);
	tstart = clock() - tstart;
	if (_img_pass == 0) printf("JPG Decode time (trans): %u ms\r\n", tstart);
	vTaskDelay(1000 / portTICK_RATE_MS);

	tft_jpg_image(0, 0, 0, NULL, test2_jpg_start, test2_jpg_end-test2_jpg_start);
	vTaskDelay(2000 / portTICK_RATE_MS);
	tft_jpg_image(0, 0, 0, NULL, test3_jpg_start, test3_jpg_end-test3_jpg_start);
	vTaskDelay(2000 / portTICK_RATE_MS);
	tft_jpg_image(0, 0, 0, NULL, test4_jpg_start, test4_jpg_end-test4_jpg_start);

	_fg = TFT_BLACK;
	TFT_setFont(TOONEY32_FONT, NULL);
	_transparent = 1;

	TFT_print("JPG IMAGE", 150, _height-40);
	_fg = TFT_CYAN;
	TFT_print("JPG IMAGE", 152, _height-38);
	vTaskDelay(5000 / portTICK_RATE_MS);

	tft_use_trans = 1;
	tstart = clock();
	tft_bmp_image(0, 0, NULL, tiger_bmp_start, tiger_bmp_end-tiger_bmp_start);
	tstart = clock() - tstart;
	if (_img_pass == 0) printf("BMP Decode time (trans): %u ms\r\n", tstart);
	vTaskDelay(2000 / portTICK_RATE_MS);

	tft_use_trans = 0;
	tstart = clock();
	tft_bmp_image(0, 0, NULL, tiger_bmp_start, tiger_bmp_end-tiger_bmp_start);
	tstart = clock() - tstart;
	if (_img_pass == 0) printf("BMP Decode time (no trans): %u ms\r\n", tstart);
	vTaskDelay(2000 / portTICK_RATE_MS);


	_fg = TFT_BLACK;
	TFT_print("BMP IMAGE", 150, _height-40);
	_fg = TFT_CYAN;
	TFT_print("BMP IMAGE", 152, _height-38);

	_transparent = 0;
	vTaskDelay(5000 / portTICK_RATE_MS);

	_img_pass++;
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
	TFT_print("Rotated text 270 deg", 6, _height-70);
	rotation = 90;
	_fg = random_color();
	TFT_print("Rotated text 90 deg", _width-12, 80);
	rotation = 0;

	_fg = random_color();
	TFT_setFont(COMIC24_FONT, NULL);
	TFT_print("ESP32 ", CENTER, 100);

	_fg = random_color();
	TFT_setFont(DEJAVU18_FONT, NULL);
	TFT_print("2.4 GHz Wi-Fi and Bluetooth combo chip", CENTER, 130);

	TFT_setFont(TOONEY32_FONT, NULL);
	_fg = random_color();
	TFT_print("Welcome to ESP32", CENTER, CENTER);

	TFT_setFont(FONT_7SEG, NULL);
	_fg = TFT_BLUE;
	set_font_atrib(14, 3, 1, (color_t){0,0,128});
}

//===============
void tft_test() {
	TFT_setRotation(LANDSCAPE);
	rotation = 0;
	_wrap = 0;
	_transparent = 0;
	_forceFixed = 0;
	tft_resetclipwin();

	color_t color = TFT_BLACK, fill = TFT_BLACK;
	struct tm* tm_info;
    char buffer[16];
	time_t now;
	uint16_t rpos = 100;
	time(&now);
	tm_info = localtime(&now);
	int nsec = -1;
	uint64_t nn = 0;
	uint8_t xdir = 0;

	while (1) {
		time(&now);
		tm_info = localtime(&now);

		if ((clock() * portTICK_RATE_MS) > nn ) {
			nn = (clock() * portTICK_RATE_MS) + 60000;
			disp_images();
			disp_frame();
			disp_text();
		}

		if ((rpos % 50) == 0) color = random_color();
		TFT_fillRect(rpos, 34, 16, 16, TFT_BLACK);
		if (xdir == 0) {
			rpos++;
			if (rpos > 364) {
				rpos -= 2;
				xdir = 1;
			}
		}
		else {
			rpos--;
			if (rpos < 100) {
				rpos += 2;
				xdir = 0;
			}
		}
		TFT_fillRect(rpos, 34, 16, 16, color);

		if (tm_info->tm_sec != nsec) {
			nsec = tm_info->tm_sec;
			disp_text();

			color = random_color();
			fill = random_color();
			TFT_fillCircle(50,50,30, fill);
			TFT_drawCircle(50,50,30, color);

			color = random_color();
			fill = random_color();
			TFT_fillCircle(50,_height-50,30, fill);
			TFT_drawCircle(50,_height-50,30, color);

			color = random_color();
			fill = random_color();
			TFT_fillCircle(_width-50,50,30, fill);
			TFT_drawCircle(_width-50,50,30, color);

			color = random_color();
			fill = random_color();
			TFT_fillCircle(_width-50,_height-50,30, fill);
			TFT_drawCircle(_width-50,_height-50,30, color);

			strftime(buffer, 16, "%H:%M:%S", tm_info);
			TFT_print(buffer, CENTER, 220);
		}

    	vTaskDelay(50 / portTICK_RATE_MS);
	}
}

/*
//---------------------------
static esp_err_t initfs(void)
{
	const char *TAG = "SPI_FS";

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

	struct stat sb;
	if ((stat("tiger240.jpg", &sb) == 0) && (sb.st_size > 0)) {
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Saving jpg file");
    FILE *f = fopen("/spiflash/tiger240.jpg", "wb");
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
    ESP_LOGI(TAG, "File written");
    return ESP_OK;

    // Unmount FATFS
    //ESP_LOGI(TAG, "Unmounting FAT filesystem");
    //ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));

    //ESP_LOGI(TAG, "Done");
}
*/

//-------------
void app_main()
{
    esp_err_t ret;

    /*
	ret = initfs();
    assert(ret==ESP_OK);
    */
	tft_line = malloc(TFT_LINEBUF_MAX_SIZE*3+128);

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
        .clock_speed_hz=2500000,                //Initial clock out at 2.5 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=-1,                       //we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           //external CS pin
		.flags=SPI_DEVICE_HALFDUPLEX,           //Set half duplex mode (Full duplex mode can also be set by commenting this line
												// but we don't need full duplex in  this example
		// We can use pre_cb callback to activate DC, but we are handling DC in low-level functions, so it is not necessary
        //.pre_cb=ili_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
		.queue_size = 2,
    };

#if USE_TOUCH
	spi_nodma_device_interface_config_t tsdevcfg={
        .clock_speed_hz=2500000,                //Clock out at 2.5 MHz
        .mode=0,                                //SPI mode 2
        .spics_io_num=PIN_NUM_TCS,              //Touch CS pin
		.spics_ext_io_num=-1,                   //Not using the external CS
		.command_bits=8,                        //1 byte command
    };
#endif

	vTaskDelay(500 / portTICK_RATE_MS);
	printf("\r\n===================================\r\n");
	printf("spi_master_nodma demo, LoBo 04/2017\r\n");
	printf("===================================\r\n\r\n");

	//Initialize the SPI bus and attach the LCD to the SPI bus
    ret=spi_nodma_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: bus initialized\r\n");

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

	// Test select/deselect
	ret = spi_nodma_device_select(tsspi, 1);
    assert(ret==ESP_OK);
	ret = spi_nodma_device_deselect(tsspi);
    assert(ret==ESP_OK);

	printf("SPI: attached TS device, speed=%u\r\n", spi_nodma_get_speed(tsspi));
#endif

	disp_spi = spi;
	ts_spi = tsspi;

	//Initialize the LCD
	printf("SPI: display init...\r\n");
    ili_init(spi);
	printf("OK\r\n");
	vTaskDelay(500 / portTICK_RATE_MS);
	
	//display_test(spi, tsspi);

	spi_nodma_set_speed(spi, 40000000);

	printf("\r\nGraphics demo started\r\n");
	tft_test();
}
