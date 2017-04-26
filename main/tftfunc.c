/*
 * 
 * HIGH SPEED LOW LEVEL DISPLAY FUNCTIONS USING DIRECT TRANSFER MODE
 *
*/

#include <string.h>
#include "esp_system.h"
#include "tftfunc.h"

// ### set it to 16 for ILI9341; 24 for ILI9488 ###
uint8_t color_bits = 24;

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

//----------------------------------------------------------------------------
void IRAM_ATTR color2rgb(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (color & 0xF800) >> 11;
    *g = (color & 0x07E0) >> 5;
    *b = color & 0x001F;
    *r = (*r * 255) / 31;
    *g = (*g * 255) / 63;
    *b = (*b * 255) / 31;
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
        uint8_t r, g, b;
        color2rgb(color, &r, &g, &b);
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
            }
            else {
            	clr = (uint16_t)color[0] | ((uint16_t)color[1] << 8);
                color2rgb(clr, &red, &green, &blue);
            }
		}
		else {
			// get color data from buffer
            if (color_bits == 16) {
                wd |= (uint32_t)color[count<<1];
                wd |= (uint32_t)color[(count<<1)+1] << 8;
                wbits += 16;
            }
            else {
            	clr = (uint16_t)color[(count<<1)] | ((uint16_t)color[(count<<1)+1] << 8);
                color2rgb(clr, &red, &green, &blue);
            }
		}
        if (color_bits == 24) {
            if (wbits == 32) {
                handle->host->hw->data_buf[idx] = wd;
                wd = 0;
                wbits = 0;
                idx++;
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
            wd |= (uint32_t)(blue) << wbits;
            if (wbits == 32) {
                handle->host->hw->data_buf[idx] = wd;
                wd = 0;
                wbits = 0;
                idx++;
            }
            wbits += 8;
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

