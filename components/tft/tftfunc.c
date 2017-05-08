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

uint8_t tft_use_trans = 0;

color_t *tft_line = NULL;
uint16_t _width = 320;
uint16_t _height = 240;

spi_nodma_device_handle_t disp_spi = NULL;
spi_nodma_device_handle_t ts_spi = NULL;

static spi_nodma_transaction_t tft_trans;

// Start spi bus transfer of given number of bits
//----------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_start(spi_nodma_device_handle_t handle, int bits) {
	// Load send buffer
	spi_nodma_host_t *host = handle->host;
	host->hw->user.usr_mosi_highpart = 0;
	host->hw->mosi_dlen.usr_mosi_dbitlen = bits-1;
	host->hw->user.usr_mosi = 1;
	if (handle->cfg.flags & SPI_DEVICE_HALFDUPLEX) {
		host->hw->miso_dlen.usr_miso_dbitlen = 0;
		host->hw->user.usr_miso = 0;
	}
	else {
		host->hw->miso_dlen.usr_miso_dbitlen = bits-1;
		host->hw->user.usr_miso = 1;
	}
	// Start transfer
	host->hw->cmd.usr = 1;
}

// Send 1 byte display command
//----------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_cmd(spi_nodma_device_handle_t handle, int8_t cmd) {
	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);

	// Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);

    handle->host->hw->data_buf[0] = (uint32_t)cmd;
    disp_spi_transfer_start(handle, 8);

    // Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
}

//--------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_cmd_data(spi_nodma_device_handle_t handle, int8_t cmd, uint8_t *data, uint32_t len) {
	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);

    // Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);

    handle->host->hw->data_buf[0] = (uint32_t)cmd;
    disp_spi_transfer_start(handle, 8);
    // Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);

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
			handle->host->hw->data_buf[idx] = wd;
    		break;
    	}
		if (bidx == 32) {
			handle->host->hw->data_buf[idx] = wd;
			idx++;
			bidx = 0;
			wd = 0;
		}
    	if (idx == 16) {
    		// SPI buffer full, send data
			disp_spi_transfer_start(handle, bits);
    		// Wait for SPI bus ready
			while (handle->host->hw->cmd.usr);
    		
			bits = 0;
    		idx = 0;
			bidx = 0;
    	}
    }
    if (bits > 0) disp_spi_transfer_start(handle, bits);
	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
}

// Set the address window for display write & read commands
//------------------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_addrwin(spi_nodma_device_handle_t handle, uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2) {
	uint32_t wd;

	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
	disp_spi_transfer_cmd(handle, TFT_CASET);

	wd = (uint32_t)(x1>>8);
	wd |= (uint32_t)(x1&0xff) << 8;
	wd |= (uint32_t)(x2>>8) << 16;
	wd |= (uint32_t)(x2&0xff) << 24;

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	handle->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(handle, 32);

	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);

	disp_spi_transfer_cmd(handle, TFT_PASET);

	wd = (uint32_t)(y1>>8);
	wd |= (uint32_t)(y1&0xff) << 8;
	wd |= (uint32_t)(y2>>8) << 16;
	wd |= (uint32_t)(y2&0xff) << 24;

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	handle->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(handle, 32);

    // Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
}

// Set one display pixel to given color, address window already set
//---------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_pixel(spi_nodma_device_handle_t handle, color_t color) {
	uint32_t wd;

	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
	disp_spi_transfer_cmd(handle, TFT_RAMWR);

#if (COLOR_BITS == 16)
    wd = (uint32_t)(color.r & 0xF8);
    wd |= (uint32_t)(color.g & 0xE0) << 3;
    wd |= (uint32_t)(color.g & 0x1C) << 11;
    wd |= (uint32_t)(color.b & 0xF8) << 5;
#else
    wd = (uint32_t)color.r;
    wd |= (uint32_t)color.g << 8;
    wd |= (uint32_t)color.b << 16;
#endif

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	handle->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(handle, COLOR_BITS);
    // Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
}

// Set one display pixel at given coordinates to given color
//----------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_set_pixel(spi_nodma_device_handle_t handle, uint16_t x, uint16_t y, color_t color) {
	disp_spi_transfer_addrwin(handle, x, x+1, y, y+1);
	disp_spi_transfer_pixel(handle, color);
}

// If rep==true  repeat sending color data to display 'len' times
// If rep==false send 'len' color data from color buffer to display
// address window must be already set
//---------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_color_rep(spi_nodma_device_handle_t handle, color_t *color, uint32_t len, uint8_t rep)
{
	uint32_t count = 0;	// sent color counter
	uint32_t cidx=0;	// color buffer index
	uint32_t wd = 0;	// used to place color data to 32-bit register in hw spi buffer

	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);

	// RAM write command
	disp_spi_transfer_cmd(handle, TFT_RAMWR);

	// Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	spi_nodma_host_t *host = handle->host;
	host->hw->user.usr_mosi_highpart = 0;
	host->hw->user.usr_mosi = 1;
#if (COLOR_BITS == 16)
	host->hw->mosi_dlen.usr_mosi_dbitlen = 15;
#else
	host->hw->mosi_dlen.usr_mosi_dbitlen = 23;
#endif
	if (handle->cfg.flags & SPI_DEVICE_HALFDUPLEX) {
		host->hw->miso_dlen.usr_miso_dbitlen = 0;
		host->hw->user.usr_miso = 0;
	}
	else {
#if (COLOR_BITS == 16)
		host->hw->miso_dlen.usr_miso_dbitlen = 15;
#else
		host->hw->miso_dlen.usr_miso_dbitlen = 23;
#endif
		host->hw->user.usr_miso = 1;
	}

	while (count < len) {
    	// ==== Push color data to spi buffer ====
		// ** Get color data from color buffer **
#if (COLOR_BITS == 16)
		wd |= (uint32_t)((color[cidx].r & 0xF8) << wbits);
		wd |= (uint32_t)(color[cidx].g & 0xE0) << (wbits+3);
		wd |= (uint32_t)(color[cidx].g & 0x1C) << (wbits+11);
		wd |= (uint32_t)(color[cidx].b & 0xF8) << (wbits+5);
		wbits += 16;
        if (wbits == 32) {
            handle->host->hw->data_buf[idx] = wd;
            wd = 0;
            wbits = 0;
            idx++;
        }
#else
		wd = (uint32_t)(color[cidx].r & 0xFC);
		wd |= (uint32_t)(color[cidx].g & 0xFC) << 8;
		wd |= (uint32_t)(color[cidx].b & 0xFC) << 16;
		handle->host->hw->data_buf[0] = wd;
		// Start transfer
		host->hw->cmd.usr = 1;
		// Wait for SPI bus ready
		while (handle->host->hw->cmd.usr);
#endif
        // Increment color buffer index if not repeating color
        if (rep == 0) cidx++;
        // Increment sent colors counter
    	count++;
    }
}

// Draw pixel on TFT on x,y position using given color
//------------------------------------------------------------------------
void IRAM_ATTR drawPixel(int16_t x, int16_t y, color_t color, uint8_t sel)
{
	if (sel) {
		if (spi_nodma_device_select(disp_spi, 0)) return;
	}
	taskDISABLE_INTERRUPTS();

	// ** Send pixel color **
	disp_spi_set_pixel(disp_spi, x, y, color);

	if (sel) spi_nodma_device_deselect(disp_spi);
	taskENABLE_INTERRUPTS();
}

// Write 'len' color data to TFT 'window' (x1,y2),(x2,y2) from given buffer
//----------------------------------------------------------------------------------
void IRAM_ATTR send_data(int x1, int y1, int x2, int y2, uint32_t len, color_t *buf)
{
	if (spi_nodma_device_select(disp_spi, 0)) return;

	// ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

	// ** Send pixel buffer **
	disp_spi_transfer_color_rep(disp_spi, buf, len, 0);

	spi_nodma_device_deselect(disp_spi);
}

// ** Send color data using transaction mode **
//---------------------------------------------------------------------------------------------
esp_err_t IRAM_ATTR send_data_trans(int x1, int y1, int x2, int y2, uint32_t len, color_t *buf)
{
	if (spi_nodma_device_select(disp_spi, 0)) return ESP_ERR_INVALID_STATE;
	//vTaskSuspendAll ();

	// ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

	// ** RAM write command
	// Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);

    disp_spi->host->hw->data_buf[0] = (uint32_t)TFT_RAMWR;
    disp_spi_transfer_start(disp_spi, 8);

	//Transaction descriptors
    memset(&tft_trans, 0, sizeof(spi_nodma_transaction_t));

    tft_trans.tx_buffer=buf;
    tft_trans.length=len*3*8;  //Data length, in bits

    // Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);
    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

    //Queue transaction.
    return spi_device_queue_trans(disp_spi, &tft_trans, 1000*portTICK_RATE_MS);
}

// ** Send color data using transaction mode **
//-----------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_color_rep_trans(int x1, int y1, int x2, int y2, color_t color, uint32_t len)
{
	if (spi_nodma_device_select(disp_spi, 0)) return;
	//vTaskSuspendAll ();

    spi_nodma_transaction_t *rtrans;

    // ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

	// ** RAM write command
	// Set DC to 0 (command mode);
    gpio_set_level(PIN_NUM_DC, 0);
    disp_spi->host->hw->data_buf[0] = (uint32_t)TFT_RAMWR;
    disp_spi_transfer_start(disp_spi, 8);

    uint32_t size = len;
    if (size > TFT_LINEBUF_MAX_SIZE) size = TFT_LINEBUF_MAX_SIZE;
    for (int i=0; i<size; i++) {
    	tft_line[i] = color;
    }

    // Wait for SPI bus ready
	while (disp_spi->host->hw->cmd.usr);
	// Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	int tosend = len;
	while (tosend > 0);{
	    memset(&tft_trans, 0, sizeof(spi_nodma_transaction_t));
		tft_trans.tx_buffer = (uint8_t *)tft_line;
		tft_trans.length = size*3*8;  //Data length, in bits
		//Queue transaction.
		spi_device_queue_trans(disp_spi, &tft_trans, 1000*portTICK_RATE_MS);
	    spi_device_get_trans_result(disp_spi, &rtrans, 1000*portTICK_RATE_MS);

		tosend -= size;
		size = tosend;
	    if (size > TFT_LINEBUF_MAX_SIZE) size = TFT_LINEBUF_MAX_SIZE;
    }

	spi_nodma_device_deselect(disp_spi);
}

//------------------------------------
esp_err_t IRAM_ATTR send_data_finish()
{
    spi_nodma_transaction_t *rtrans;
    // Wait for transaction to be done
    esp_err_t ret = spi_device_get_trans_result(disp_spi, &rtrans, 1000*portTICK_RATE_MS);
	spi_nodma_device_deselect(disp_spi);
	return ret;
}


// Write 'len' 16-bit color data to TFT 'window' (x1,y2),(x2,y2)
// uses the buffer to fill the color values
//-------------------------------------------------------------------------------------------
void IRAM_ATTR TFT_pushColorRep(int x1, int y1, int x2, int y2, color_t color, uint32_t len)
{
	if (spi_nodma_device_select(disp_spi, 0)) return;

	// ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

	// ** Send repeated pixel color **
	disp_spi_transfer_color_rep(disp_spi, &color, len, 1);

	spi_nodma_device_deselect(disp_spi);
}

// Reads pixels/colors from the TFT's GRAM
//-----------------------------------------------------------------------------------------
int IRAM_ATTR read_data(int x1, int y1, int x2, int y2, int len, uint8_t *buf, uint8_t sel)
{
	spi_nodma_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction
	memset(buf, 0, len*sizeof(color_t));

	if (sel) {
		if (spi_nodma_device_select(disp_spi, 0)) return -2;
	}
	//vTaskSuspendAll ();

    // ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

    // ** GET pixels/colors **
	disp_spi_transfer_cmd(disp_spi, TFT_RAMRD);

    t.length=0;                //Send nothing
    t.tx_buffer=NULL;
    t.rxlength=8*((len*3)+1);  //Receive size in bits
    t.rx_buffer=buf;
    t.user = (void*)1;

	spi_nodma_transfer_data(disp_spi, &t); // Transmit using direct mode

	if (sel) spi_nodma_device_deselect(disp_spi);
    //xTaskResumeAll ();

    return 0;
}

// Reads one pixel/color from the TFT's GRAM
//------------------------------------------------------------
color_t IRAM_ATTR readPixel(int16_t x, int16_t y, uint8_t sel)
{
    uint8_t color_buf[sizeof(color_t)+1] = {0};
	read_data(x, y, x+1, y+1, 1, color_buf, sel);
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
	taskDISABLE_INTERRUPTS();

	// send command byte & receive 2 byte response
    t.rxlength=8*2;
    t.rx_buffer=&rxdata;
	t.command = type;

	ret = spi_nodma_transfer_data(ts_spi, &t);    // Transmit using direct mode

	spi_nodma_device_deselect(ts_spi);
	taskENABLE_INTERRUPTS();

	if (ret) return 0;

	return (((uint16_t)(rxdata[0] << 8) | (uint16_t)(rxdata[1])) >> 4);
}


