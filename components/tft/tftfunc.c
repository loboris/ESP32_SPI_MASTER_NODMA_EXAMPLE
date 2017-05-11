/*
 * 
 * HIGH SPEED LOW LEVEL DISPLAY FUNCTIONS USING DIRECT TRANSFER MODE
 *
*/

#include <string.h>
#include "esp_system.h"
#include "tftfunc.h"
#include "freertos/task.h"

// ### set it to 16 for ILI9341; 24 for ILI9488 ###

// RGB to GRAYSCALE constants
// 0.2989  0.5870  0.1140
#define GS_FACT_R 0.2989
#define GS_FACT_G 0.4870
#define GS_FACT_B 0.2140


uint8_t tft_use_trans = 1;
uint8_t tft_in_trans = 0;
uint8_t COLOR_BITS = 24;
uint8_t gray_scale = 0;
uint32_t max_rdclock = 16000000;

color_t *tft_line = NULL;
uint16_t _width = 320;
uint16_t _height = 240;

// Set display type
uint8_t tft_disp_type = DISP_TYPE_ILI9488;


spi_nodma_device_handle_t disp_spi = NULL;
spi_nodma_device_handle_t ts_spi = NULL;

static spi_nodma_transaction_t tft_trans;

//-------------------------------------
esp_err_t IRAM_ATTR wait_trans_finish()
{
    if (!tft_in_trans) return ESP_OK;

    spi_nodma_transaction_t *rtrans;
    // Wait for transaction to be done
    esp_err_t ret = spi_device_get_trans_result(disp_spi, &rtrans, 1000*portTICK_RATE_MS);
    tft_in_trans = 0;
    return ret;
}

//-------------------------------
esp_err_t IRAM_ATTR disp_select()
{
	esp_err_t ret = wait_trans_finish();
	if (ret != ESP_OK) return ret;

	return spi_nodma_device_select(disp_spi, 0);
}

//---------------------------------
esp_err_t IRAM_ATTR disp_deselect()
{
	esp_err_t ret = wait_trans_finish();
	if (ret != ESP_OK) return ret;

	return spi_nodma_device_deselect(disp_spi);
}

// Start spi bus transfer of given number of bits
//-------------------------------------------------------
static void IRAM_ATTR disp_spi_transfer_start(int bits) {
	// Load send buffer
	disp_spi->host->hw->user.usr_mosi_highpart = 0;
	disp_spi->host->hw->mosi_dlen.usr_mosi_dbitlen = bits-1;
	disp_spi->host->hw->user.usr_mosi = 1;
	if (disp_spi->cfg.flags & SPI_DEVICE_HALFDUPLEX) {
		disp_spi->host->hw->miso_dlen.usr_miso_dbitlen = 0;
		disp_spi->host->hw->user.usr_miso = 0;
	}
	else {
		disp_spi->host->hw->miso_dlen.usr_miso_dbitlen = bits-1;
		disp_spi->host->hw->user.usr_miso = 1;
	}
	// Start transfer
	disp_spi->host->hw->cmd.usr = 1;
    // Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);
}

// Send 1 byte display command, display must be selected
//------------------------------------------------
void IRAM_ATTR disp_spi_transfer_cmd(int8_t cmd) {
	// Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);

	// Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);

    disp_spi->host->hw->data_buf[0] = (uint32_t)cmd;
    disp_spi_transfer_start(8);
}

// Send command with data to display, display must be selected
//----------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_cmd_data(int8_t cmd, uint8_t *data, uint32_t len) {
	// Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);

    // Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);

    disp_spi->host->hw->data_buf[0] = (uint32_t)cmd;
    disp_spi_transfer_start(8);

	if (len == 0) return;

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	uint8_t idx=0, bidx=0;
	uint32_t bits=0;
	uint32_t count=0;
	uint32_t wd = 0;
	while (count < len) {
		// get data byte from buffer
		wd |= (uint32_t)data[count] << bidx;
    	count++;
    	bits += 8;
		bidx += 8;
    	if (count == len) {
    		disp_spi->host->hw->data_buf[idx] = wd;
    		break;
    	}
		if (bidx == 32) {
			disp_spi->host->hw->data_buf[idx] = wd;
			idx++;
			bidx = 0;
			wd = 0;
		}
    	if (idx == 16) {
    		// SPI buffer full, send data
			disp_spi_transfer_start(bits);
    		
			bits = 0;
    		idx = 0;
			bidx = 0;
    	}
    }
    if (bits > 0) disp_spi_transfer_start(bits);
}

// Set the address window for display write & read commands, display must be selected
//---------------------------------------------------------------------------------------------------
static void IRAM_ATTR disp_spi_transfer_addrwin(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2) {
	uint32_t wd;

	// Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);
	disp_spi_transfer_cmd(TFT_CASET);

	wd = (uint32_t)(x1>>8);
	wd |= (uint32_t)(x1&0xff) << 8;
	wd |= (uint32_t)(x2>>8) << 16;
	wd |= (uint32_t)(x2&0xff) << 24;

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	disp_spi->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(32);

	disp_spi_transfer_cmd(TFT_PASET);

	wd = (uint32_t)(y1>>8);
	wd |= (uint32_t)(y1&0xff) << 8;
	wd |= (uint32_t)(y2>>8) << 16;
	wd |= (uint32_t)(y2&0xff) << 24;

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	disp_spi->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(32);
}

// Convert color to gray scale
//----------------------------------------------
static color_t IRAM_ATTR color2gs(color_t color)
{
    color_t _color;
    float gs_clr;

    gs_clr = GS_FACT_R * color.r + GS_FACT_G * color.g + GS_FACT_B * color.b;

    _color.r = (uint8_t)gs_clr;
	_color.g = (uint8_t)gs_clr;
	_color.b = (uint8_t)gs_clr;

	return _color;
}

//---------------------------------------
static uint32_t pack_color(color_t color)
{
	uint32_t color16 = 0;
	uint8_t *buf16 = (uint8_t *)(&color16);

	// Convert RGB 18-bit, 3 byte colors to RGB 16-bit, 2 byte format
	buf16[0] = color.r & 0xF8;
	buf16[0] |= (color.g & 0xE0) >> 5;
	buf16[1] = (color.g & 0x1C) << 3;
	buf16[1] |= (color.b & 0xF8) >> 3;

	return color16;
}

// Set display pixel at given coordinates to given color
//------------------------------------------------------------------------
void IRAM_ATTR drawPixel(int16_t x, int16_t y, color_t color, uint8_t sel)
{
	if (!(disp_spi->cfg.flags & SPI_DEVICE_HALFDUPLEX)) return;

	if (sel) {
		if (disp_select()) return;
	}
	else wait_trans_finish();

	disp_spi_transfer_addrwin(x, x+1, y, y+1);

    color_t _color;
	uint32_t wd;

	disp_spi_transfer_cmd(TFT_RAMWR);

	if (gray_scale) _color = color2gs(color);
	else _color = color;

	if (COLOR_BITS == 16) {
		wd = pack_color(_color);
	}
	else {
		wd = (uint32_t)_color.r;
		wd |= (uint32_t)_color.g << 8;
		wd |= (uint32_t)_color.b << 16;
	}

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	disp_spi->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(COLOR_BITS);

    if (sel) disp_deselect();
}

// If rep==true  repeat sending color data to display 'len' times
// If rep==false send 'len' color data from color buffer to display
// ** Device must already be selected and address window set **
//--------------------------------------------------------------------------------
static void IRAM_ATTR _TFT_pushColorRep(color_t *color, uint32_t len, uint8_t rep)
{
	if (!(disp_spi->cfg.flags & SPI_DEVICE_HALFDUPLEX)) return;

	uint32_t count = 0;	// sent color counter
	uint32_t cidx = 0;	// color buffer index
	uint32_t wd = 0;	// used to place color data to 32-bit register in hw spi buffer
	float gs_clr = 0;

	// * Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);

	// * Prepare spi device
	disp_spi->host->hw->user.usr_mosi_highpart = 0;
	disp_spi->host->hw->user.usr_miso_highpart = 0;
	disp_spi->host->hw->user.usr_mosi = 1;
	disp_spi->host->hw->miso_dlen.usr_miso_dbitlen = 0;
	disp_spi->host->hw->user.usr_miso = 0;

	// * Send RAM write command
    gpio_set_level(PIN_NUM_DC, 0);						// set DC to 0 (command mode);
    disp_spi->host->hw->data_buf[0] = (uint32_t)TFT_RAMWR;
	disp_spi->host->hw->mosi_dlen.usr_mosi_dbitlen = 7;	// send 8 bits
	disp_spi->host->hw->cmd.usr = 1;					// Start transfer
	while (disp_spi->host->hw->cmd.usr);				// Wait for SPI bus ready

	gpio_set_level(PIN_NUM_DC, 1);						// Set DC to 1 (data mode);


	while (count < len) {
    	// ==== Push color data to spi buffer ====
		// ** Get color data from color buffer **
		if (COLOR_BITS == 16) {
			if (gray_scale) wd = pack_color(color2gs(color[cidx]));
			else wd = pack_color(color[cidx]);
		}
		else {
			if (gray_scale) {
			    gs_clr = GS_FACT_R * color[cidx].r + GS_FACT_G * color[cidx].g + GS_FACT_B * color[cidx].b;
				wd = (uint32_t)gs_clr;
				wd |=  (uint32_t)gs_clr << 8;
				wd |= (uint32_t)gs_clr << 16;
			}
			else {
				wd = (uint32_t)color[cidx].r;
				wd |= (uint32_t)color[cidx].g << 8;
				wd |= (uint32_t)color[cidx].b << 16;
			}
		}
        disp_spi->host->hw->data_buf[0] = wd;
		disp_spi->host->hw->mosi_dlen.usr_mosi_dbitlen = COLOR_BITS-1;	// set number of bits to be sent
        disp_spi->host->hw->cmd.usr = 1;								// Start transfer
		while (disp_spi->host->hw->cmd.usr);							// Wait for SPI bus ready

    	count++;	// Increment sent colors counter
        if (rep == 0) cidx++;
    }
}

// ** Send color data using DMA mode **
//------------------------------------------------------------------------
static void IRAM_ATTR _TFT_pushColorRep_trans(color_t color, uint32_t len)
{
    uint32_t size, tosend;
    esp_err_t ret = ESP_OK;
    color_t _color;

	if (gray_scale) _color = color2gs(color);
	else _color = color;

	size = len;
    if (size > TFT_LINEBUF_MAX_SIZE) size = TFT_LINEBUF_MAX_SIZE;

	uint16_t *buf16 = (uint16_t *)tft_line;
    for (uint32_t n=0; n<size;n++) {
	    if (COLOR_BITS == 16) buf16[n] = (uint16_t)pack_color(_color);
	    else tft_line[n] = _color;
    }

	// ** RAM write command
	// Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);
    disp_spi->host->hw->data_buf[0] = (uint32_t)TFT_RAMWR;
    disp_spi_transfer_start(8);

	// Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	tosend = len;
	while (tosend > 0) {
	    memset(&tft_trans, 0, sizeof(spi_nodma_transaction_t));
		size = tosend;
	    if (size > TFT_LINEBUF_MAX_SIZE) size = TFT_LINEBUF_MAX_SIZE;

	    tft_trans.tx_buffer = (uint8_t *)tft_line;
	    //Set data length, in bits
	    if (COLOR_BITS == 16) tft_trans.length = size * 2 * 8;
	    else  tft_trans.length = size * 3 * 8;

	    //Queue transaction.
		ret = spi_device_queue_trans(disp_spi, &tft_trans, 1000*portTICK_RATE_MS);
	    tft_in_trans = 1;
		if (ret != ESP_OK) break;

		ret = wait_trans_finish();
		if (ret != ESP_OK) break;

	    tosend -= size;
    }
}

// Write 'len' 16-bit color data to TFT 'window' (x1,y2),(x2,y2)
// uses the buffer to fill the color values
//-------------------------------------------------------------------------------------------
void IRAM_ATTR TFT_pushColorRep(int x1, int y1, int x2, int y2, color_t color, uint32_t len)
{
	if (disp_select() != ESP_OK) return;

	// ** Send address window **
	disp_spi_transfer_addrwin(x1, x2, y1, y2);

	if (tft_use_trans) _TFT_pushColorRep_trans(color, len);
	else _TFT_pushColorRep(&color, len, 1);

	disp_deselect();
}

// Write 'len' color data to TFT 'window' (x1,y2),(x2,y2) from given buffer
//-----------------------------------------------------------------------------------
void IRAM_ATTR send_data(int x1, int y1, int x2, int y2, uint32_t len, color_t *buf)
{
	if (disp_select() != ESP_OK) return;

	// ** Send address window **
	disp_spi_transfer_addrwin(x1, x2, y1, y2);

	if (tft_use_trans) {
	    // ** Send color data using transaction mode **
		// ** RAM write command
		// Set DC to 0 (command mode);
	    gpio_set_level(PIN_NUM_DC, 0);
	    disp_spi->host->hw->data_buf[0] = (uint32_t)TFT_RAMWR;
	    disp_spi_transfer_start(8);

		//Transaction descriptors
	    memset(&tft_trans, 0, sizeof(spi_nodma_transaction_t));

	    uint32_t size = 0;
	    if (COLOR_BITS == 16) {
			uint16_t *buf16 = (uint16_t *)buf;
			for (int n=0; n<len; n++) {
				if (gray_scale) buf16[n] = (uint16_t)pack_color(color2gs(buf[n]));
				else buf16[n] = (uint16_t)pack_color(buf[n]);
			}
	    	size = len * 2;
	    }
	    else {
	    	if (gray_scale) {
	    		for (int n=0; n<len;n++) {
					buf[n] = color2gs(buf[n]);
	    		}
	    	}
	    	size = len * 3;
	    }

	    tft_trans.tx_buffer = (uint8_t *)buf;
	    tft_trans.length = size * 8;  //Data length, in bits

	    // Set DC to 1 (data mode);
		gpio_set_level(PIN_NUM_DC, 1);

	    //Queue transaction.
	    spi_device_queue_trans(disp_spi, &tft_trans, 1000*portTICK_RATE_MS);
	    tft_in_trans = 1;
	}
	else {
		// ** Send pixel buffer **
		_TFT_pushColorRep(buf, len, 0);
		disp_deselect();
	}
}

// Reads pixels/colors from the TFT's GRAM
//----------------------------------------------------------------------------
int IRAM_ATTR read_data(int x1, int y1, int x2, int y2, int len, uint8_t *buf)
{
	spi_nodma_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction
	memset(buf, 0, len*sizeof(color_t));


	if (disp_deselect() != ESP_OK) return -1;

	uint32_t current_clock = spi_nodma_get_speed(disp_spi);
	if (max_rdclock < current_clock) spi_nodma_set_speed(disp_spi, max_rdclock);

	if (disp_select() != ESP_OK) return -2;

	// ** Send address window **
	disp_spi_transfer_addrwin(x1, x2, y1, y2);

    // ** GET pixels/colors **
	disp_spi_transfer_cmd(TFT_RAMRD);

    t.length=0;                //Send nothing
    t.tx_buffer=NULL;
    t.rxlength=8*((len*3)+1);  //Receive size in bits
    t.rx_buffer=buf;
    t.user = (void*)1;

	spi_nodma_transfer_data(disp_spi, &t); // Receive using direct mode

	disp_deselect();

	if (max_rdclock < current_clock) spi_nodma_set_speed(disp_spi, current_clock);

    return 0;
}

// Reads one pixel/color from the TFT's GRAM
//-----------------------------------------------
color_t IRAM_ATTR readPixel(int16_t x, int16_t y)
{
    uint8_t color_buf[sizeof(color_t)+1] = {0};

    read_data(x, y, x+1, y+1, 1, color_buf);

    color_t color;
	color.r = color_buf[1];
	color.g = color_buf[2];
	color.b = color_buf[3];
	return color;
}

//---------------------------------------------
uint16_t IRAM_ATTR touch_get_data(uint8_t type)
{
	esp_err_t ret;
	spi_nodma_transaction_t t;
	memset(&t, 0, sizeof(t));            //Zero out the transaction
	uint8_t rxdata[2] = {0};

	if (spi_nodma_device_select(ts_spi, 0)) return 0;

	// send command byte & receive 2 byte response
    t.rxlength=8*2;
    t.rx_buffer=&rxdata;
	t.command = type;

	ret = spi_nodma_transfer_data(ts_spi, &t);    // Transmit using direct mode

	spi_nodma_device_deselect(ts_spi);

	if (ret) return 0;

	return (((uint16_t)(rxdata[0] << 8) | (uint16_t)(rxdata[1])) >> 4);
}

