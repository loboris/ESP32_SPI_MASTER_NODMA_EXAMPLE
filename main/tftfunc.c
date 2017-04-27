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
uint8_t color_bits = 24;
uint16_t *tft_line = NULL;
uint16_t _width = 320;
uint16_t _height = 240;

spi_nodma_device_handle_t disp_spi = NULL;
spi_nodma_device_handle_t ts_spi = NULL;


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
//----------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_pixel(spi_nodma_device_handle_t handle, uint16_t color) {
	uint32_t wd;

	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
	disp_spi_transfer_cmd(handle, TFT_RAMWR);

    if (color_bits == 16) {
        //wd = (uint32_t)color;
        wd = (uint32_t)(color >> 8);
        wd |= (uint32_t)(color & 0xff) << 8;
    }
    else {
        uint8_t r = (((color & 0xF800) >> 11) * 255) / 31;
        uint8_t g = (((color & 0x07E0) >> 5) * 255) / 63;
        uint8_t b = ((color & 0x001F) * 255) / 31;
        wd = r;
        wd |= (uint32_t)(g) << 8;
        wd |= (uint32_t)(b) << 16;
    }

    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	handle->host->hw->data_buf[0] = wd;
    disp_spi_transfer_start(handle, color_bits);

    // Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
}

// Set one display pixel at given coordinates to given color
//-----------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_set_pixel(spi_nodma_device_handle_t handle, uint16_t x, uint16_t y, uint16_t color) {
	disp_spi_transfer_addrwin(handle, x, x+1, y, y+1);
	disp_spi_transfer_pixel(handle, color);
}

// If rep==true  repeat sending color data to display 'len' times
// If rep==false send 'len' color data from color buffer to display
// address window must be already set
//---------------------------------------------------------------------------------------------------------------------
void IRAM_ATTR disp_spi_transfer_color_rep(spi_nodma_device_handle_t handle, uint8_t *color, uint32_t len, uint8_t rep)
{
	uint8_t idx = 0;
	uint32_t count = 0;
	uint32_t wd = 0;
	uint32_t bits = 0;
    uint8_t wbits = 0;
    uint8_t red=0, green=0, blue=0;
    uint16_t clr = 0;

	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);

	disp_spi_transfer_cmd(handle, TFT_RAMWR);

	// Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

    if ((rep) && (color_bits == 24)) {
        // prepare color data for ILI9844 in repeat color mode
        clr = (uint16_t)color[0] | ((uint16_t)color[1] << 8);
        red = (((clr & 0xF800) >> 11) * 255) / 31;
        green = (((clr & 0x07E0) >> 5) * 255) / 63;
        blue = ((clr & 0x001F) * 255) / 31;
    }

	while (count < len) {
        // ** Check if we have enough bits in spi buffer for the next color
    	if ((bits + color_bits) > 512) {
    	    if (wbits) handle->host->hw->data_buf[idx] = wd;
    		// ** SPI buffer full, send data
			disp_spi_transfer_start(handle, bits);
    		// Wait for SPI bus ready
    		while (handle->host->hw->cmd.usr);

			bits = 0;
            wbits = 0;
			idx = 0;
    	}

    	// ==== Push color data to spi buffer ====
    	if (rep) {
			// get color data from color pointer
            if (color_bits == 16) {
                wd |= (uint32_t)color[1];
                wd |= (uint32_t)color[0] << 8;
                wbits += 16;
            }
		}
		else {
			// get color data from buffer
            if (color_bits == 16) {
                wd |= (uint32_t)color[count<<1];
                wd |= (uint32_t)color[(count<<1)+1] << 8;
                wbits += 16;
            }
		}
        if (color_bits == 24) {
            if (rep == 0) {
                clr = (uint16_t)color[(count<<1)+1] | ((uint16_t)color[(count<<1)] << 8);
                red = (((clr & 0xF800) >> 11) * 255) / 31;
                green = (((clr & 0x07E0) >> 5) * 255) / 63;
                blue = ((clr & 0x001F) * 255) / 31;
            }
            wd |= (uint32_t)(red) << wbits;
            wbits += 8;
            if (wbits == 32) {
                handle->host->hw->data_buf[idx] = wd;
                wd = 0;
                wbits = 0;
                idx++;
            }
            wd |= (uint32_t)(green) << wbits;
            wbits += 8;
            if (wbits == 32) {
                handle->host->hw->data_buf[idx] = wd;
                wd = 0;
                wbits = 0;
                idx++;
            }
            wd |= (uint32_t)(blue) << wbits;
            wbits += 8;
            if (wbits == 32) {
                handle->host->hw->data_buf[idx] = wd;
                wd = 0;
                wbits = 0;
                idx++;
            }
        }
        else {
            if (wbits == 32) {
                handle->host->hw->data_buf[idx] = wd;
                wd = 0;
                wbits = 0;
                idx++;
            }
        }

        // Increment color & bits counters
    	count++;
    	bits += color_bits;
    	if (count == len) break;
    }
    if (wbits) {
        handle->host->hw->data_buf[idx] = wd;
    }

    if (bits > 0) disp_spi_transfer_start(handle, bits);
	// Wait for SPI bus ready
	while (handle->host->hw->cmd.usr);
}

// Reads pixels/colors from the TFT's GRAM
//-----------------------------------------------------------------------------------------------------------------------
int IRAM_ATTR disp_spi_read_data(spi_nodma_device_handle_t handle, int x1, int y1, int x2, int y2, int len, uint8_t *buf)
{
	memset(buf, 0, len*2);

	uint8_t *rbuf = malloc((len*3)+1);
    if (!rbuf) return -1;

    memset(rbuf, 0, (len*3)+1);

    // ** Send address window **
	disp_spi_transfer_addrwin(handle, x1, x2, y1, y2);

    // ** GET pixels/colors **
	disp_spi_transfer_cmd(handle, TFT_RAMRD);

	// Receive data
    esp_err_t ret;
    // Set DC to 1 (data mode);
	gpio_set_level(PIN_NUM_DC, 1);

	spi_nodma_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction
    t.length=0;                //Send notning
    t.tx_buffer=NULL;
    t.rxlength=8*((len*3)+1);  //Receive size in bits
    t.rx_buffer=rbuf;
    //t.user=(void*)1;         //D/C needs to be set to 1

	ret = spi_nodma_transfer_data(handle, &t); // Transmit using direct mode

	if (ret == ESP_OK) {
		/*
		for (int i=25; i<48; i+=3) {
			printf("[%02x,%02x,%02x] ",rbuf[i],rbuf[i+1],rbuf[i+2]);
		}
		printf("\r\n");
		*/
		int idx = 0;
		uint16_t color;
		for (int i=1; i<(len*3); i+=3) {
			color = (uint16_t)((uint16_t)((rbuf[i] & 0xF8) << 8) | (uint16_t)((rbuf[i+1] & 0xFC) << 3) | (uint16_t)(rbuf[i+2] >> 3));
			buf[idx++] = color >> 8;
			buf[idx++] = color & 0xFF;
		}
	}
    free(rbuf);

    return ret;
}

// Draw pixel on TFT on x,y position using given color
//-------------------------------------------------------------------------
void IRAM_ATTR drawPixel(int16_t x, int16_t y, uint16_t color, uint8_t sel)
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

// Write 'len' 16-bit color data to TFT 'window' (x1,y2),(x2,y2)
// uses the buffer to fill the color values
//-------------------------------------------------------------------------------------------
void IRAM_ATTR TFT_pushColorRep(int x1, int y1, int x2, int y2, uint16_t color, uint32_t len)
{
	uint16_t ccolor = color;
	if (spi_nodma_device_select(disp_spi, 0)) return;
	//vTaskSuspendAll ();

	// ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

	// ** Send repeated pixel color **
	disp_spi_transfer_color_rep(disp_spi, (uint8_t *)&ccolor, len, 1);

	spi_nodma_device_deselect(disp_spi);
    //xTaskResumeAll ();
}

// Write 'len' 16-bit color data to TFT 'window' (x1,y2),(x2,y2) from given buffer
//-----------------------------------------------------------------------------------
void IRAM_ATTR send_data(int x1, int y1, int x2, int y2, uint32_t len, uint16_t *buf)
{
	if (spi_nodma_device_select(disp_spi, 0)) return;
	//vTaskSuspendAll ();

	// ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

	// ** Send pixel buffer **
	disp_spi_transfer_color_rep(disp_spi, (uint8_t *)buf, len, 0);

	spi_nodma_device_deselect(disp_spi);
    //xTaskResumeAll ();
}

// Reads one pixel/color from the TFT's GRAM
//--------------------------------------
uint16_t readPixel(int16_t x, int16_t y)
{
	uint8_t inbuf[4] = {0};
	spi_nodma_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction

	if (spi_nodma_device_select(disp_spi, 0)) return 0;
	taskDISABLE_INTERRUPTS();

	// ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x, x+1, y, y+1);

    // ** GET pixel color **
	disp_spi_transfer_cmd(disp_spi, TFT_RAMRD);

    t.length=0;                //Send nothing
    t.tx_buffer=NULL;
    t.rxlength=8*4;  //Receive size in bits
    t.rx_buffer=inbuf;
    t.user = (void*)1;

	spi_nodma_transfer_data(disp_spi, &t); // Transmit using direct mode

	spi_nodma_device_deselect(disp_spi);
    taskENABLE_INTERRUPTS();

	//printf("READ DATA: [%02x, %02x, %02x, %02x]\r\n", inbuf[0],inbuf[1],inbuf[2],inbuf[3]);
    return (uint16_t)((uint16_t)((inbuf[1] & 0xF8) << 8) | (uint16_t)((inbuf[2] & 0xFC) << 3) | (uint16_t)(inbuf[3] >> 3));
}

// Reads pixels/colors from the TFT's GRAM
//------------------------------------------------------------------
int read_data(int x1, int y1, int x2, int y2, int len, uint8_t *buf)
{
	spi_nodma_transaction_t t;
    memset(&t, 0, sizeof(t));  //Zero out the transaction
	memset(buf, 0, len*2);

	uint8_t *rbuf = malloc((len*3)+1);
    if (!rbuf) return -1;

    memset(rbuf, 0, (len*3)+1);

	if (spi_nodma_device_select(disp_spi, 0)) return -2;
	//vTaskSuspendAll ();

    // ** Send address window **
	disp_spi_transfer_addrwin(disp_spi, x1, x2, y1, y2);

    // ** GET pixels/colors **
	disp_spi_transfer_cmd(disp_spi, TFT_RAMRD);

    t.length=0;                //Send nothing
    t.tx_buffer=NULL;
    t.rxlength=8*((len*3)+1);  //Receive size in bits
    t.rx_buffer=rbuf;
    t.user = (void*)1;

	spi_nodma_transfer_data(disp_spi, &t); // Transmit using direct mode

	spi_nodma_device_deselect(disp_spi);
    //xTaskResumeAll ();

    int idx = 0;
    uint16_t color;
    for (int i=1; i<(len*3); i+=3) {
    	color = (uint16_t)((uint16_t)((rbuf[i] & 0xF8) << 8) | (uint16_t)((rbuf[i+1] & 0xFC) << 3) | (uint16_t)(rbuf[i+2] >> 3));
    	buf[idx++] = color >> 8;
    	buf[idx++] = color & 0xFF;
    }
    free(rbuf);

    return 0;
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


