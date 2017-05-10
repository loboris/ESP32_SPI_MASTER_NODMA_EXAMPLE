
### Non DMA version of the spi_master driver

---

Based on esp-idf **spi_master** driver, modified by **LoBo** [https://github.com/loboris] 06/2017

The example uses **wear leveling FAT file system** and the latest **esp-idf** commit has to be used (5. May 2017 or later)

---

#### Main features

*  Transfers data to SPI device in direct mode, not using DMA
*  All configuration options (bus, device, transaction) are the same as in spi_master driver
*  Transfers uses the semaphore (taken in select function & given in deselect function) to protect the transfer
*  Number of the devices attached to the bus which uses hardware CS can be 3 (**NO_CS**)
*  Additional devices which uses software CS can be attached to the bus, up to **NO_DEV**
*  **spi_bus_initialize** & **spi_bus_remove** functions are removed, spi bus is initialized/removed in *spi_nodma_bus_add_device*/*spi_nodma_bus_remove_device* when needed
*  **spi_nodma_bus_add_device** function has added parameter *bus_config* and automatically initializes spi bus device if not already initialized
*  **spi_nodma_bus_remove_device** automatically removes spi bus device if no other devices are attached to it.
*  Devices can have individual bus_configs, so different *mosi*, *miso*, *sck* pins can be configured for each device. Reconfiguring the bus is done automaticaly in **spi_nodma_device_select** function
*  **spi_nodma_device_select** & **spi_nodma_device_deselect** functions handles devices configuration changes and software **CS**
*  Some helper functions are added (**get_speed**, **set_speed**, ...)
*  All structures are available in header file for easy creation of user low level spi functions. See **tftfunc.c** source for examples.
*  Transimt and receive lenghts are limited only by available memory

Main driver's function is **spi_nodma_transfer_data()**

*  **TRANSMIT** 8-bit data to spi device from *trans->tx_buffer* or *trans->tx_data* (trans->lenght/8 bytes) and **RECEIVE** data to *trans->rx_buffer* or *trans->rx_data* (trans->rx_length/8 bytes)
*  Lengths must be **8-bit** multiples! (for now)
*  If trans->rx_buffer is NULL or trans->rx_length is 0, only transmits data
*  If trans->tx_buffer is NULL or trans->length is 0, only receives data
*  If the device is in duplex mode (*SPI_DEVICE_HALFDUPLEX* flag **not** set), data are transmitted and received simultaneously.
*  If the device is in half duplex mode (*SPI_DEVICE_HALFDUPLEX* flag **is** set), data are received after transmission
*  **address**, **command** and **dummy bits** are transmitted before data phase **if** set in device's configuration and **if** 'trans->length' and 'trans->rx_length' are **not** both 0
*  If configured, devices **pre_cb** callback is called before and **post_cb** after the transmission
*  If device was not previously selected, it will be selected before transmission and deselected after transmission.

---

**Complete function decsriptions are available in the header file** *spi_master_nodma.h*

---

---

You can place **spi_master_nodma.c** in **<esp-idf_path>/components/driver** directory
and **spi_master_nodma.h** in **<esp-idf_path>/components/driver/include/driver** directory

In that case replace **#include "spi_master_nodma.h"** with **#include "driver/spi_master_nodma.h"** in **spi_master_nodma.c**

---

### Example: SPI Display driver

#### TFT display driver features

* TFT library with many drawing functions and fonts is included.
* Full support for ILI9341 & ILI9488 based TFT modules in 4-wire SPI mode.
* 18-bit (RGB) color mode (default or 16-bit backed RGB565 color mode (only on ILI9341)
* DMA transfer mode on some functions to improve speed
* Grayscale mode can be selected
* Graphics functions: drawpixel, line, linebyangle, rect, roundrect, circle, ellipse, triangle, arc, poly, star ... All shapes can be filled or not. Drawing can be limitid to clipping window.
* Fonts: fixed width an proportional; 7 fonts embeded, unlimited number of fonts from file, 7-segment vector font with variable width/height. Proportional fonts can be used in fixed width mode.
* String write function: on x,y possition, center, left/right/top/bottom justify. Transparent or opaque writing, optional wrapping. Writting can be limitid to clipping window.
* Images: jpeg, bmp bitmap images.
* Touch screen supported (with XPT2046 controllers)
* Read from display memory supported

**The code is not fully tested with ILI9431 yet.**


To run the example, attach ILI9341 or ILI9488 based display module to ESP32. Default pins used are:
* mosi: 23
* miso: 19
*  sck: 18
*   CS:  5 (display CS)
*   DC: 26 (display DC)
*  TCS: 25 (touch screen CS)

---

**If you have ILI9488, set** `tft_disp_type = DISP_TYPE_ILI9488` in *spi_master_demo.c*

**If you have ILI9341, set** `tft_disp_type = DISP_TYPE_ILI9341` in *spi_master_demo.c*

**If you want to use different pins, change them in** *tftfunc.h*

**if you want to use the touch screen functions, set** `#define USE_TOUCH	1` in *tftfunc.h*

Using *make menuconfig* **select tick rate 1000** ( → Component config → FreeRTOS → Tick rate (Hz) ) to get more accurate timings

---

* This example tests accessing ILI9341 or ILI9488 based display using **spi_master_nodma** driver
* Basics functions are executed first and timings at several spi clock speeds are printed.
* Four different JPG images are shown on screen to demonstrate jpeg decoding and scaling
* One BMP image is shown on screen to demonstrate bmp decoding
* Text and graphics are drawn on screen to demonstrate some drawing functions and text/fonts functionality
* Full color mode and grayscale mode are alternated on every pass
* On first 4 passes timings are printed for different operating modes

 
If Touch screen is available, reading the touch coordinates (non calibrated) is also demonstrated. Keep the display touched until the info is printed. 
This is active only during the first phase (Display test).

---

![Example on ILI9488 480x320 display](https://raw.githubusercontent.com/loboris/ESP32_SPI_MASTER_NODMA_EXAMPLE/master/demo.jpg)

---

**Example output:**

```
I (1695) cpu_start: Pro cpu start user code
I (1748) cpu_start: Starting scheduler on PRO CPU.
I (1751) [SPI_FS]: Mounting FAT filesystem
E (1753) [SPI_FS]: FATFS mounted
E (1756) [SPI_FS]: JPG image found on file system

===================================
spi_master_nodma demo, LoBo 05/2017
===================================

SPI: display device added to spi bus
SPI: attached display device, speed=8000000
SPI: bus uses native pins: true
SPI: display init...
OK
-------------
 Disp clock =  8.00 MHz (requested:  8.00)
      Lines =  1396  ms (320 lines of 480 pixels)
 Read check      OK, line 218
     Pixels =  4151  ms (480x320)
        Cls =   475  ms (480x320)
-------------
-------------
 Disp clock = 10.00 MHz (requested: 10.00)
      Lines =  1395  ms (320 lines of 480 pixels)
 Read check      OK, line 293
     Pixels =  3572  ms (480x320)
        Cls =   382  ms (480x320)
-------------
-------------
 Disp clock = 16.00 MHz (requested: 16.00)
      Lines =  1394  ms (320 lines of 480 pixels)
 Read check      OK, line 300
     Pixels =  2718  ms (480x320)
        Cls =   244  ms (480x320)
-------------
### MAX READ SPI CLOCK set to 16.00 MHz ###
-------------
 Disp clock = 26.67 MHz (requested: 30.00)
      Lines =  1393  ms (320 lines of 480 pixels)
 Read check     Err, on line 232 at 17 (Read clock = 30.00 MHz)
     Pixels =  2131  ms (480x320)
        Cls =   152  ms (480x320)
-------------
-------------
 Disp clock = 40.00 MHz (requested: 40.00)
      Lines =  1392  ms (320 lines of 480 pixels)
 Read check      OK, line 163 (Read clock = 16.00 MHz)
     Pixels =  1835  ms (480x320)
        Cls =   106  ms (480x320)
-------------

Graphics demo started
---------------------

         DMA mode: OFF
   GRAYSCALE mode: OFF

Clear screen time: 226 ms
Display line time: 720 us
  JPG Decode time: 411 ms
Decode JPG (file): 411 ms
  BMP Decode time: 768 ms

         DMA mode: ON
   GRAYSCALE mode: OFF

Clear screen time: 106 ms
Display line time: 341 us
  JPG Decode time: 211 ms
Decode JPG (file): 211 ms
  BMP Decode time: 768 ms

         DMA mode: OFF
   GRAYSCALE mode: ON

Clear screen time: 453 ms
Display line time: 1586 us
  JPG Decode time: 742 ms
Decode JPG (file): 742 ms
  BMP Decode time: 1103 ms

         DMA mode: ON
   GRAYSCALE mode: ON

Clear screen time: 106 ms
Display line time: 958 us
  JPG Decode time: 557 ms
Decode JPG (file): 557 ms
  BMP Decode time: 1103 ms

```
