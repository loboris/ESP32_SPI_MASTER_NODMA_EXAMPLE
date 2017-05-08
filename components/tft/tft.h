/*
 * High level TFT functions
 * Author:  LoBo 04/2017, https://github/loboris
 * 
 */

#ifndef _TFT_H_
#define _TFT_H_

#include <stdlib.h>

// Constants for ellipse function
#define TFT_ELLIPSE_UPPER_RIGHT 0x01
#define TFT_ELLIPSE_UPPER_LEFT  0x02
#define TFT_ELLIPSE_LOWER_LEFT  0x04
#define TFT_ELLIPSE_LOWER_RIGHT 0x08

// Constants for Arc function
// number representing the maximum angle (e.g. if 100, then if you pass in start=0 and end=50, you get a half circle)
// this can be changed with setArcParams function at runtime
#define DEFAULT_ARC_ANGLE_MAX 360
// rotational offset in degrees defining position of value 0 (-90 will put it at the top of circle)
// this can be changed with setAngleOffset function at runtime
#define DEFAULT_ANGLE_OFFSET -90

// Color definitions constants
const color_t TFT_BLACK;
const color_t TFT_NAVY;
const color_t TFT_DARKGREEN;
const color_t TFT_DARKCYAN;
const color_t TFT_MAROON;
const color_t TFT_PURPLE;
const color_t TFT_OLIVE;
const color_t TFT_LIGHTGREY;
const color_t TFT_DARKGREY;
const color_t TFT_BLUE;
const color_t TFT_GREEN;
const color_t TFT_CYAN;
const color_t TFT_RED;
const color_t TFT_MAGENTA;
const color_t TFT_YELLOW;
const color_t TFT_WHITE;
const color_t TFT_ORANGE;
const color_t TFT_GREENYELLOW;
const color_t TFT_PINK;


#define INVERT_ON		1
#define INVERT_OFF		0

// Screen orientation constants
#define PORTRAIT	0
#define LANDSCAPE	1
#define PORTRAIT_FLIP	2
#define LANDSCAPE_FLIP	3

// Special coordinates constants
#define LASTX	-1
#define LASTY	-2
#define CENTER	-3
#define RIGHT	-4
#define BOTTOM	-4

// Embeded fonts constants
#define DEFAULT_FONT	0
#define DEJAVU18_FONT	1
#define DEJAVU24_FONT	2
#define UBUNTU16_FONT	3
#define COMIC24_FONT	4
#define MINYA24_FONT	5
#define TOONEY32_FONT	6
#define FONT_7SEG		7
#define USER_FONT		8  // font will be read from file


uint8_t     orientation;    // current screen orientation
uint16_t    rotation;       // current font rotation angle (0~395)
uint8_t     _transparent;   // if not 0 draw fonts transparent
color_t     _fg;            // current foreground color for fonts
color_t     _bg;            // current background for non transparent fonts
uint8_t     _wrap;          // if not 0 wrap long text to the new line, else clip
uint8_t     _forceFixed;    // if not zero force drawing proportional fonts with fixed width

/*
 * Draw pixel at given x,y coordinates
 * 
 * Params:
 *       x: horizontal position
 *       y: vertical position
 *   color: pixel color as 16-bit coded (RGB565) value
 *     sel: if not 0 activate CS before and deactivat after sending pixel data to display
 *          when sending multiple pixels it is faster to activate the CS first,
 *          send all pixels an deactivate CS after all pixela was sent
*/
void TFT_drawPixel(int16_t x, int16_t y, color_t color, uint8_t sel);

/*
 * Read pixel color value from display GRAM at given x,y coordinates
 * 
 * Params:
 *       x: horizontal position
 *       y: vertical position
 * 
 * Returns:
 *      pixel color at x,y as 16-bit coded (RGB565) value
*/
color_t TFT_readPixel(int16_t x, int16_t y);

/*
 * Draw vertical line at given x,y coordinates
 * 
 * Params:
 *       x: horizontal start position
 *       y: vertical start position
 *       h: line height in pixels
 *   color: line color as 16-bit coded (RGB565) value
*/
void TFT_drawFastVLine(int16_t x, int16_t y, int16_t h, color_t color);

/*
 * Draw horizontal line at given x,y coordinates
 * 
 * Params:
 *       x: horizontal start position
 *       y: vertical start position
 *       w: line width in pixels
 *   color: line color as 16-bit coded (RGB565) value
*/
void TFT_drawFastHLine(int16_t x, int16_t y, int16_t w, color_t color);

/*
 * Draw line on screen
 * 
 * Params:
 *       x0: horizontal start position
 *       y0: vertical start position
 *       x1: horizontal end position
 *       y1: vertical end position
 *   color: line color as 16-bit coded (RGB565) value
*/
void TFT_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color);

/*
 * Fill given rectangular screen region with color
 * 
 * Params:
 *       x: horizontal rect start position
 *       y: vertical rect start position
 *       w: rectangle width
 *       h: rectangle height
 *   color: fill color as 16-bit coded (RGB565) value
*/
void TFT_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, color_t color);

/*
 * Draw rectangle on screen
 * 
 * Params:
 *       x: horizontal rect start position
 *       y: vertical rect start position
 *       w: rectangle width
 *       h: rectangle height
 *   color: rect line color as 16-bit coded (RGB565) value
*/
void TFT_drawRect(uint16_t x1,uint16_t y1,uint16_t w,uint16_t h, color_t color);

/*
 * Draw rectangle with rounded corners on screen
 * 
 * Params:
 *       x: horizontal rect start position
 *       y: vertical rect start position
 *       w: rectangle width
 *       h: rectangle height
 *       r: corner radius
 *   color: rectangle color as 16-bit coded (RGB565) value
*/
void TFT_drawRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, color_t color);

/*
 * Fill given rectangular screen region with rounded corners with color
 * 
 * Params:
 *       x: horizontal rect start position
 *       y: vertical rect start position
 *       w: rectangle width
 *       h: rectangle height
 *       r: corner radius
 *   color: fill color as 16-bit coded (RGB565) value
*/
void TFT_fillRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, color_t color);

/*
 * Fill the whole screen with color
 * 
 * Params:
 *   color: fill color as 16-bit coded (RGB565) value
*/
void TFT_fillScreen(color_t color);

/*
 * Draw triangle on screen
 * 
 * Params:
 *       x0: first triangle point x position
 *       y0: first triangle point y position
 *       x0: second triangle point x position
 *       y0: second triangle point y position
 *       x0: third triangle point x position
 *       y0: third triangle point y position
 *   color: triangle color as 16-bit coded (RGB565) value
*/
void TFT_drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color);

/*
 * Fill triangular screen region with color
 * 
 * Params:
 *       x0: first triangle point x position
 *       y0: first triangle point y position
 *       x0: second triangle point x position
 *       y0: second triangle point y position
 *       x0: third triangle point x position
 *       y0: third triangle point y position
 *   color: fill color as 16-bit coded (RGB565) value
*/
void TFT_fillTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color);

/*
 * Draw circle on screen
 * 
 * Params:
 *       x: circle center x position
 *       y: circle center x position
 *       r: circle radius
 *   color: circle color as 16-bit coded (RGB565) value
*/
void TFT_drawCircle(int16_t x, int16_t y, int radius, color_t color);

/*
 * Fill circle on screen with color
 * 
 * Params:
 *       x: circle center x position
 *       y: circle center x position
 *       r: circle radius
 *   color: circle fill color as 16-bit coded (RGB565) value
*/
void TFT_fillCircle(int16_t x, int16_t y, int radius, color_t color);

/*
 * Draw elipse on screen
 * 
 * Params:
 *       x0: elipse center x position
 *       y0: elipse center x position
 *       rx: elipse horizontal radius
 *       ry: elipse vertical radius
 *   option: drawing options, multiple options can be combined
                1 (TFT_ELLIPSE_UPPER_RIGHT) draw upper right corner
                2 (TFT_ELLIPSE_UPPER_LEFT)  draw upper left corner
                4 (TFT_ELLIPSE_LOWER_LEFT)  draw lower left corner
                8 (TFT_ELLIPSE_LOWER_RIGHT) draw lower right corner
             to draw the whole elipse use option value 15 (1 | 2 | 4 | 8)
 * 
 *   color: circle color as 16-bit coded (RGB565) value
*/
void TFT_draw_ellipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, color_t color, uint8_t option);

/*
 * Fill eliptical region on screen
 * 
 * Params:
 *       x0: elipse center x position
 *       y0: elipse center x position
 *       rx: elipse horizontal radius
 *       ry: elipse vertical radius
 *   option: drawing options, multiple options can be combined
                1 (TFT_ELLIPSE_UPPER_RIGHT) fill upper right corner
                2 (TFT_ELLIPSE_UPPER_LEFT)  fill upper left corner
                4 (TFT_ELLIPSE_LOWER_LEFT)  fill lower left corner
                8 (TFT_ELLIPSE_LOWER_RIGHT) fill lower right corner
             to fill the whole elipse use option value 15 (1 | 2 | 4 | 8)
 * 
 *   color: fill color as 16-bit coded (RGB565) value
*/
void TFT_draw_filled_ellipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, color_t color, uint8_t option);

int compare_colors(color_t c1, color_t c2);

void drawPolygon(int cx, int cy, int sides, int diameter, color_t color, uint8_t fill, int deg);
void drawStar(int cx, int cy, int diameter, color_t color, bool fill, float factor);

void TFT_setFont(uint8_t font, const char *font_file);

void TFT_print(char *st, int x, int y);

void tft_setclipwin(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

void tft_resetclipwin();

void set_font_atrib(uint8_t l, uint8_t w, int offset, color_t color);

void TFT_setRotation(uint8_t m);
void TFT_invertDisplay(const uint8_t mode);

color_t HSBtoRGB(float _hue, float _sat, float _brightness);

void tft_jpg_image(int x, int y, int ascale, char *fname, uint8_t *buf, int size);
int tft_bmp_image(int x, int y, char *fname, uint8_t *imgbuf, int size);

int tp_get_data(uint8_t type, int samples);

#endif
