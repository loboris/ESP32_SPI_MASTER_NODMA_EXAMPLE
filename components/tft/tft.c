/* TFT module
 *
 *  Author: LoBo (loboris@gmail.com, loboris.github)
 *
 *  Module supporting SPI TFT displays based on ILI9341 & ILI9488 controllers
*/

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "tftfunc.h"
#include "tft.h"
#include "time.h"
#include <math.h>
#include "rom/tjpgd.h"


#define DEG_TO_RAD 0.01745329252
#define RAD_TO_DEG 57.295779513
#define deg_to_rad 0.01745329252 + 3.14159265359

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#if !defined(max)
#define max(A,B) ( (A) > (B) ? (A):(B))
#endif
#if !defined(min)
#define min(A,B) ( (A) < (B) ? (A):(B))
#endif

// Color definitions constants
const color_t TFT_BLACK       = {   0,   0,   0 };
const color_t TFT_NAVY        = {   0,   0, 128 };
const color_t TFT_DARKGREEN   = {   0, 128,   0 };
const color_t TFT_DARKCYAN    = {   0, 128, 128 };
const color_t TFT_MAROON      = { 128,   0,   0 };
const color_t TFT_PURPLE      = { 128,   0, 128 };
const color_t TFT_OLIVE       = { 128, 128,   0 };
const color_t TFT_LIGHTGREY   = { 192, 192, 192 };
const color_t TFT_DARKGREY    = { 128, 128, 128 };
const color_t TFT_BLUE        = {   0,   0, 255 };
const color_t TFT_GREEN       = {   0, 255,   0 };
const color_t TFT_CYAN        = {   0, 255, 255 };
const color_t TFT_RED         = { 255,   0,   0 };
const color_t TFT_MAGENTA     = { 255,   0, 255 };
const color_t TFT_YELLOW      = { 255, 255,   0 };
const color_t TFT_WHITE       = { 255, 255, 255 };
const color_t TFT_ORANGE      = { 255, 165,   0 };
const color_t TFT_GREENYELLOW = { 173, 255,  47 };
const color_t TFT_PINK        = { 255, 192, 203 };

//#define tft_color(color) ( (uint16_t)((color >> 8) | (color << 8)) )
#define swap(a, b) { int16_t t = a; a = b; b = t; }

typedef struct {
	uint8_t 	*font;
	uint8_t 	x_size;
	uint8_t 	y_size;
	uint8_t	    offset;
	uint16_t	numchars;
    uint8_t     bitmap;
	color_t     color;
} Font;

typedef struct {
      uint8_t charCode;
      int adjYOffset;
      int width;
      int height;
      int xOffset;
      int xDelta;
      uint16_t dataPtr;
} propFont;

typedef struct {
	uint16_t        x1;
	uint16_t        y1;
	uint16_t        x2;
	uint16_t        y2;
} dispWin_t;

static dispWin_t dispWin = {
  .x1 = 0,
  .y1 = 0,
  .x2 = 320,
  .y2 = 240,
};


extern uint8_t tft_DefaultFont[];
extern uint8_t tft_Dejavu18[];
extern uint8_t tft_Dejavu24[];
extern uint8_t tft_Ubuntu16[];
extern uint8_t tft_Comic24[];
extern uint8_t tft_minya24[];
extern uint8_t tft_tooney32[];

//static uint8_t tp_initialized = 0;	// touch panel initialized flag

//static uint8_t *userfont = NULL;

uint8_t orientation = PORTRAIT;	// screen orientation
uint16_t rotation = 0;			// font rotation
uint8_t	_transparent = 0;
color_t	_fg = {   0, 255,   0 };
color_t _bg = {   0,   0,   0 };
uint8_t	_wrap = 0;				// character wrapping to new line
uint8_t	_forceFixed = 0;

static int		TFT_X  = 0;
static int		TFT_Y  = 0;
static int		TFT_OFFSET  = 0;

static Font		cfont;
static propFont	fontChar;

uint32_t tp_calx = 7472920;
uint32_t tp_caly = 122224794;

//static float _arcAngleMax = DEFAULT_ARC_ANGLE_MAX;
//static float _angleOffset = DEFAULT_ANGLE_OFFSET;

//------------------------------------------
int compare_colors(color_t c1, color_t c2) {
	if (COLOR_BITS == 24) {
		if ((c1.r & 0xFC) != (c2.r & 0xFC)) return 1;
		if ((c1.b & 0xFC) != (c2.b & 0xFC)) return 1;
	}
	else {
		if ((c1.r & 0xF8) != (c2.r & 0xF8)) return 1;
		if ((c1.b & 0xF8) != (c2.b & 0xF8)) return 1;
	}
	if ((c1.g & 0xFC) != (c2.g & 0xFC)) return 1;
	return 0;
}

// ================ Basics drawing functions ===================================
// Only functions which actually sends data to display
// All drawings are clipped to 'dispWin'

// draw color pixel on screen
//---------------------------------------------------------------------
void TFT_drawPixel(int16_t x, int16_t y, color_t color, uint8_t sel) {

  if ((x < dispWin.x1) || (y < dispWin.y1) || (x > dispWin.x2) || (y > dispWin.y2)) return;

  drawPixel(x, y, color, sel);
}

//-------------------------------------------
color_t TFT_readPixel(int16_t x, int16_t y) {

  if ((x < dispWin.x1) || (y < dispWin.y1) || (x > dispWin.x2) || (y > dispWin.y2)) return TFT_BLACK;

  return readPixel(x, y);
}

//-----------------------------------------------------------------------
void TFT_drawFastVLine(int16_t x, int16_t y, int16_t h, color_t color) {
	// clipping
	if ((x < dispWin.x1) || (x > dispWin.x2) || (y > dispWin.y2)) return;
	if (y < dispWin.y1) {
		h -= (dispWin.y1 - y);
		y = dispWin.y1;
	}
	if (h < 0) h = 0;
	if ((y + h) > (dispWin.y2+1)) h = dispWin.y2 - y + 1;
	if (h == 0) h = 1;
	TFT_pushColorRep(x, y, x, y+h-1, color, (uint32_t)h);
}

//-----------------------------------------------------------------------
void TFT_drawFastHLine(int16_t x, int16_t y, int16_t w, color_t color) {
	// clipping
	if ((y < dispWin.y1) || (x > dispWin.x2) || (y > dispWin.y2)) return;
	if (x < dispWin.x1) {
		w -= (dispWin.x1 - x);
		x = dispWin.x1;
	}
	if (w < 0) w = 0;
	if ((x + w) > (dispWin.x2+1)) w = dispWin.x2 - x + 1;
	if (w == 0) w = 1;

	TFT_pushColorRep(x, y, x+w-1, y, color, (uint32_t)w);
}

// fill a rectangle
//-----------------------------------------------------------------------------
void TFT_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, color_t color) {
	// clipping
	if ((x >= dispWin.x2) || (y > dispWin.y2)) return;

	if (x < dispWin.x1) {
		w -= (dispWin.x1 - x);
		x = dispWin.x1;
	}
	if (y < dispWin.y1) {
		h -= (dispWin.y1 - y);
		y = dispWin.y1;
	}
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	if ((x + w) > (dispWin.x2+1)) w = dispWin.x2 - x + 1;
	if ((y + h) > (dispWin.y2+1)) h = dispWin.y2 - y + 1;
	if (w == 0) w = 1;
	if (h == 0) h = 1;
	TFT_pushColorRep(x, y, x+w-1, y+h-1, color, (uint32_t)(h*w));
}

//-----------------------------------
void TFT_fillScreen(color_t color) {
	TFT_pushColorRep(0, 0, _width-1, _height-1, color, (uint32_t)(_height*_width));
}

// ^^^============= Basics drawing functions ================================^^^


// ================ Graphics drawing functions ==================================

//--------------------------------------------------------------------------------
void TFT_drawRect(uint16_t x1,uint16_t y1,uint16_t w,uint16_t h, color_t color) {
  TFT_drawFastHLine(x1,y1,w, color);
  TFT_drawFastVLine(x1+w-1,y1,h, color);
  TFT_drawFastHLine(x1,y1+h-1,w, color);
  TFT_drawFastVLine(x1,y1,h, color);
}

//-------------------------------------------------------------------------------------------------
static void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, color_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	disp_select();
	while (x < y) {
		if (f >= 0) {
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;
		if (cornername & 0x4) {
			TFT_drawPixel(x0 + x, y0 + y, color, 0);
			TFT_drawPixel(x0 + y, y0 + x, color, 0);
		}
		if (cornername & 0x2) {
			TFT_drawPixel(x0 + x, y0 - y, color, 0);
			TFT_drawPixel(x0 + y, y0 - x, color, 0);
		}
		if (cornername & 0x8) {
			TFT_drawPixel(x0 - y, y0 + x, color, 0);
			TFT_drawPixel(x0 - x, y0 + y, color, 0);
		}
		if (cornername & 0x1) {
			TFT_drawPixel(x0 - y, y0 - x, color, 0);
			TFT_drawPixel(x0 - x, y0 - y, color, 0);
		}
	}
	disp_deselect();
}

// Used to do circles and roundrects
//----------------------------------------------------------------------------------------------------------------
static void fillCircleHelper(int16_t x0, int16_t y0, int16_t r,	uint8_t cornername, int16_t delta, color_t color)
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;
	int16_t ylm = x0 - r;

	while (x < y) {
		if (f >= 0) {
			if (cornername & 0x1) TFT_drawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
			if (cornername & 0x2) TFT_drawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
			ylm = x0 - y;
			y--;
			ddF_y += 2;
			f += ddF_y;
		}
		x++;
		ddF_x += 2;
		f += ddF_x;

		if ((x0 - x) > ylm) {
			if (cornername & 0x1) TFT_drawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
			if (cornername & 0x2) TFT_drawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
		}
	}
}

// Draw a rounded rectangle
//----------------------------------------------------------------------------------------------
void TFT_drawRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, color_t color)
{
	// smarter version
	TFT_drawFastHLine(x + r, y, w - 2 * r, color);			// Top
	TFT_drawFastHLine(x + r, y + h - 1, w - 2 * r, color);	// Bottom
	TFT_drawFastVLine(x, y + r, h - 2 * r, color);			// Left
	TFT_drawFastVLine(x + w - 1, y + r, h - 2 * r, color);	// Right

	// draw four corners
	drawCircleHelper(x + r, y + r, r, 1, color);
	drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
	drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
	drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
}

// Fill a rounded rectangle
//----------------------------------------------------------------------------------------------
void TFT_fillRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, color_t color)
{
	// smarter version
	TFT_fillRect(x + r, y, w - 2 * r, h, color);

	// draw four corners
	fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
	fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}



// Bresenham's algorithm - thx wikipedia - speed enhanced by Bodmer this uses
// the eficient FastH/V Line draw routine for segments of 2 pixels or more
//-------------------------------------------------------------------------------
void TFT_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color)
{
  if (x0 == x1) {
	  if (y0 <= y1) TFT_drawFastVLine(x0, y0, y1-y0, color);
	  else TFT_drawFastVLine(x0, y1, y0-y1, color);
	  return;
  }
  if (y0 == y1) {
	  if (x0 <= x1) TFT_drawFastHLine(x0, y0, x1-x0, color);
	  else TFT_drawFastHLine(x1, y0, x0-x1, color);
	  return;
  }

  int steep = 0;
  if (abs(y1 - y0) > abs(x1 - x0)) steep = 1;
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }
  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  int16_t dx = x1 - x0, dy = abs(y1 - y0);;
  int16_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

  if (y0 < y1) ystep = 1;

  // Split into steep and not steep for FastH/V separation
  if (steep) {
    for (; x0 <= x1; x0++) {
      dlen++;
      err -= dy;
      if (err < 0) {
        err += dx;
        if (dlen == 1) TFT_drawPixel(y0, xs, color, 1);
        else TFT_drawFastVLine(y0, xs, dlen, color);
        dlen = 0; y0 += ystep; xs = x0 + 1;
      }
    }
    if (dlen) TFT_drawFastVLine(y0, xs, dlen, color);
  }
  else
  {
    for (; x0 <= x1; x0++) {
      dlen++;
      err -= dy;
      if (err < 0) {
        err += dx;
        if (dlen == 1) TFT_drawPixel(xs, y0, color, 1);
        else TFT_drawFastHLine(xs, y0, dlen, color);
        dlen = 0; y0 += ystep; xs = x0 + 1;
      }
    }
    if (dlen) TFT_drawFastHLine(xs, y0, dlen, color);
  }
}

/*
//-----------------------------------------------------------------------------------------------
void drawLineByAngle(int16_t x, int16_t y, int16_t angle, uint16_t length, color_t color)
{
	TFT_drawLine(
		x,
		y,
		x + length * cos((angle + _angleOffset) * DEG_TO_RAD),
		y + length * sin((angle + _angleOffset) * DEG_TO_RAD), color);
}

//--------------------------------------------------------------------------------------------------------
void DrawLineByAngle(int16_t x, int16_t y, int16_t angle, uint16_t start, uint16_t length, color_t color)
{
	TFT_drawLine(
		x + start * cos((angle + _angleOffset) * DEG_TO_RAD),
		y + start * sin((angle + _angleOffset) * DEG_TO_RAD),
		x + (start + length) * cos((angle + _angleOffset) * DEG_TO_RAD),
		y + (start + length) * sin((angle + _angleOffset) * DEG_TO_RAD), color);
}
*/

// Draw a triangle
//-----------------------------------------------------------------------------------------------------------------
void TFT_drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color)
{
  TFT_drawLine(x0, y0, x1, y1, color);
  TFT_drawLine(x1, y1, x2, y2, color);
  TFT_drawLine(x2, y2, x0, y0, color);
}

// Fill a triangle
//-----------------------------------------------------------------------------------------------------------------
void TFT_fillTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, color_t color)
{
  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    TFT_drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
  int32_t
    sa   = 0,
    sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    TFT_drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    TFT_drawFastHLine(a, y, b-a+1, color);
  }
}

//---------------------------------------------------------------------
void TFT_drawCircle(int16_t x, int16_t y, int radius, color_t color) {
  int f = 1 - radius;
  int ddF_x = 1;
  int ddF_y = -2 * radius;
  int x1 = 0;
  int y1 = radius;

  disp_select();
  TFT_drawPixel(x, y + radius, color, 0);
  TFT_drawPixel(x, y - radius, color, 0);
  TFT_drawPixel(x + radius, y, color, 0);
  TFT_drawPixel(x - radius, y, color, 0);
  while(x1 < y1) {
    if (f >= 0) {
      y1--;
      ddF_y += 2;
      f += ddF_y;
    }
    x1++;
    ddF_x += 2;
    f += ddF_x;
    TFT_drawPixel(x + x1, y + y1, color, 0);
    TFT_drawPixel(x - x1, y + y1, color, 0);
    TFT_drawPixel(x + x1, y - y1, color, 0);
    TFT_drawPixel(x - x1, y - y1, color, 0);
    TFT_drawPixel(x + y1, y + x1, color, 0);
    TFT_drawPixel(x - y1, y + x1, color, 0);
    TFT_drawPixel(x + y1, y - x1, color, 0);
    TFT_drawPixel(x - y1, y - x1, color, 0);
  }
  disp_deselect();
}

//---------------------------------------------------------------------
void TFT_fillCircle(int16_t x, int16_t y, int radius, color_t color) {
	TFT_drawFastVLine(x, y-radius, 2*radius+1, color);
	fillCircleHelper(x, y, radius, 3, 0, color);
}

//--------------------------------------------------------------------------------------------------------------------
static void TFT_draw_ellipse_section(uint16_t x, uint16_t y, uint16_t x0, uint16_t y0, color_t color, uint8_t option)
{
	disp_select();
    // upper right
    if ( option & TFT_ELLIPSE_UPPER_RIGHT ) TFT_drawPixel(x0 + x, y0 - y, color, 0);
    // upper left
    if ( option & TFT_ELLIPSE_UPPER_LEFT ) TFT_drawPixel(x0 - x, y0 - y, color, 0);
    // lower right
    if ( option & TFT_ELLIPSE_LOWER_RIGHT ) TFT_drawPixel(x0 + x, y0 + y, color, 0);
    // lower left
    if ( option & TFT_ELLIPSE_LOWER_LEFT ) TFT_drawPixel(x0 - x, y0 + y, color, 0);
	disp_deselect();
}

//-------------------------------------------------------------------------------------------------------
void TFT_draw_ellipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, color_t color, uint8_t option)
{
  uint16_t x, y;
  int32_t xchg, ychg;
  int32_t err;
  int32_t rxrx2;
  int32_t ryry2;
  int32_t stopx, stopy;

  rxrx2 = rx;
  rxrx2 *= rx;
  rxrx2 *= 2;

  ryry2 = ry;
  ryry2 *= ry;
  ryry2 *= 2;

  x = rx;
  y = 0;

  xchg = 1;
  xchg -= rx;
  xchg -= rx;
  xchg *= ry;
  xchg *= ry;

  ychg = rx;
  ychg *= rx;

  err = 0;

  stopx = ryry2;
  stopx *= rx;
  stopy = 0;

  while( stopx >= stopy )
  {
	TFT_draw_ellipse_section(x, y, x0, y0, color, option);
    y++;
    stopy += rxrx2;
    err += ychg;
    ychg += rxrx2;
    if ( 2*err+xchg > 0 )
    {
      x--;
      stopx -= ryry2;
      err += xchg;
      xchg += ryry2;
    }
  }

  x = 0;
  y = ry;

  xchg = ry;
  xchg *= ry;

  ychg = 1;
  ychg -= ry;
  ychg -= ry;
  ychg *= rx;
  ychg *= rx;

  err = 0;

  stopx = 0;

  stopy = rxrx2;
  stopy *= ry;


  while( stopx <= stopy )
  {
	TFT_draw_ellipse_section(x, y, x0, y0, color, option);
    x++;
    stopx += ryry2;
    err += xchg;
    xchg += ryry2;
    if ( 2*err+ychg > 0 )
    {
      y--;
      stopy -= rxrx2;
      err += ychg;
      ychg += rxrx2;
    }
  }

}

//---------------------------------------------------------------------------------------------------------------------------
static void TFT_draw_filled_ellipse_section(uint16_t x, uint16_t y, uint16_t x0, uint16_t y0, color_t color, uint8_t option)
{
    // upper right
    if ( option & TFT_ELLIPSE_UPPER_RIGHT ) TFT_drawFastVLine(x0+x, y0-y, y+1, color);
    // upper left
    if ( option & TFT_ELLIPSE_UPPER_LEFT ) TFT_drawFastVLine(x0-x, y0-y, y+1, color);
    // lower right
    if ( option & TFT_ELLIPSE_LOWER_RIGHT ) TFT_drawFastVLine(x0+x, y0, y+1, color);
    // lower left
    if ( option & TFT_ELLIPSE_LOWER_LEFT ) TFT_drawFastVLine(x0-x, y0, y+1, color);
}

//--------------------------------------------------------------------------------------------------------------
void TFT_draw_filled_ellipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, color_t color, uint8_t option)
{
  uint16_t x, y;
  int32_t xchg, ychg;
  int32_t err;
  int32_t rxrx2;
  int32_t ryry2;
  int32_t stopx, stopy;

  rxrx2 = rx;
  rxrx2 *= rx;
  rxrx2 *= 2;

  ryry2 = ry;
  ryry2 *= ry;
  ryry2 *= 2;

  x = rx;
  y = 0;

  xchg = 1;
  xchg -= rx;
  xchg -= rx;
  xchg *= ry;
  xchg *= ry;

  ychg = rx;
  ychg *= rx;

  err = 0;

  stopx = ryry2;
  stopx *= rx;
  stopy = 0;

  while( stopx >= stopy ) {
	TFT_draw_filled_ellipse_section(x, y, x0, y0, color, option);
    y++;
    stopy += rxrx2;
    err += ychg;
    ychg += rxrx2;
    if ( 2*err+xchg > 0 )
    {
      x--;
      stopx -= ryry2;
      err += xchg;
      xchg += ryry2;
    }
  }

  x = 0;
  y = ry;

  xchg = ry;
  xchg *= ry;

  ychg = 1;
  ychg -= ry;
  ychg -= ry;
  ychg *= rx;
  ychg *= rx;

  err = 0;

  stopx = 0;

  stopy = rxrx2;
  stopy *= ry;


  while( stopx <= stopy ) {
	TFT_draw_filled_ellipse_section(x, y, x0, y0, color, option);
    x++;
    stopx += ryry2;
    err += xchg;
    xchg += ryry2;
    if ( 2*err+ychg > 0 ) {
      y--;
      stopy -= rxrx2;
      err += ychg;
      ychg += rxrx2;
    }
  }
}

/*
// Adapted from: Marek Buriak (https://github.com/marekburiak/ILI9341_due)
//--------------------------------------------------------------------------------------------------------------------------------
void TFT_fillArcOffsetted(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t thickness, float start, float end, color_t color) {
	int16_t xmin = 65535, xmax = -32767, ymin = 32767, ymax = -32767;
	float cosStart, sinStart, cosEnd, sinEnd;
	float r, t;
	float startAngle, endAngle;

	startAngle = ((start / _arcAngleMax) * 360) + _angleOffset;
	endAngle = ((end / _arcAngleMax) * 360) + _angleOffset;

	while (startAngle < 0) startAngle += 360;
	while (endAngle < 0) endAngle += 360;
	while (startAngle > 360) startAngle -= 360;
	while (endAngle > 360) endAngle -= 360;

	if (startAngle > endAngle) {
		TFT_fillArcOffsetted(cx, cy, radius, thickness, ((startAngle) / (float)360) * _arcAngleMax, _arcAngleMax, color);
		TFT_fillArcOffsetted(cx, cy, radius, thickness, 0, ((endAngle) / (float)360) * _arcAngleMax, color);
	}
	else {
		// Calculate bounding box for the arc to be drawn
		cosStart = cos(startAngle * DEG_TO_RAD);
		sinStart = sin(startAngle * DEG_TO_RAD);
		cosEnd = cos(endAngle * DEG_TO_RAD);
		sinEnd = sin(endAngle * DEG_TO_RAD);

		r = radius;
		// Point 1: radius & startAngle
		t = r * cosStart;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinStart;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		// Point 2: radius & endAngle
		t = r * cosEnd;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinEnd;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		r = radius - thickness;
		// Point 3: radius-thickness & startAngle
		t = r * cosStart;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinStart;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;

		// Point 4: radius-thickness & endAngle
		t = r * cosEnd;
		if (t < xmin) xmin = t;
		if (t > xmax) xmax = t;
		t = r * sinEnd;
		if (t < ymin) ymin = t;
		if (t > ymax) ymax = t;


		// Corrections if arc crosses X or Y axis
		if ((startAngle < 90) && (endAngle > 90)) {
			ymax = radius;
		}

		if ((startAngle < 180) && (endAngle > 180)) {
			xmin = -radius;
		}

		if ((startAngle < 270) && (endAngle > 270)) {
			ymin = -radius;
		}

		// Slopes for the two sides of the arc
		float sslope = (float)cosStart / (float)sinStart;
		float eslope = (float)cosEnd / (float)sinEnd;

		if (endAngle == 360) eslope = -1000000;

		int ir2 = (radius - thickness) * (radius - thickness);
		int or2 = radius * radius;

		for (int x = xmin; x <= xmax; x++) {
			bool y1StartFound = false, y2StartFound = false;
			bool y1EndFound = false, y2EndSearching = false;
			int y1s = 0, y1e = 0, y2s = 0;
			for (int y = ymin; y <= ymax; y++)
			{
				int x2 = x * x;
				int y2 = y * y;

				if (
					(x2 + y2 < or2 && x2 + y2 >= ir2) && (
					(y > 0 && startAngle < 180 && x <= y * sslope) ||
					(y < 0 && startAngle > 180 && x >= y * sslope) ||
					(y < 0 && startAngle <= 180) ||
					(y == 0 && startAngle <= 180 && x < 0) ||
					(y == 0 && startAngle == 0 && x > 0)
					) && (
					(y > 0 && endAngle < 180 && x >= y * eslope) ||
					(y < 0 && endAngle > 180 && x <= y * eslope) ||
					(y > 0 && endAngle >= 180) ||
					(y == 0 && endAngle >= 180 && x < 0) ||
					(y == 0 && startAngle == 0 && x > 0)))
				{
					if (!y1StartFound)	//start of the higher line found
					{
						y1StartFound = true;
						y1s = y;
					}
					else if (y1EndFound && !y2StartFound) //start of the lower line found
					{
						y2StartFound = true;
						y2s = y;
						y += y1e - y1s - 1;	// calculate the most probable end of the lower line (in most cases the length of lower line is equal to length of upper line), in the next loop we will validate if the end of line is really there
						if (y > ymax - 1) // the most probable end of line 2 is beyond ymax so line 2 must be shorter, thus continue with pixel by pixel search
						{
							y = y2s;	// reset y and continue with pixel by pixel search
							y2EndSearching = true;
						}
					}
					else if (y2StartFound && !y2EndSearching)
					{
						// we validated that the probable end of the lower line has a pixel, continue with pixel by pixel search, in most cases next loop with confirm the end of lower line as it will not find a valid pixel
						y2EndSearching = true;
					}
				}
				else
				{
					if (y1StartFound && !y1EndFound) //higher line end found
					{
						y1EndFound = true;
						y1e = y - 1;
						TFT_drawFastVLine(cx + x, cy + y1s, y - y1s, color);
						if (y < 0) {
							y = abs(y); // skip the empty middle
						}
						else break;
					}
					else if (y2StartFound)
					{
						if (y2EndSearching) {
							// we found the end of the lower line after pixel by pixel search
							TFT_drawFastVLine(cx + x, cy + y2s, y - y2s, color);
							y2EndSearching = false;
							break;
						}
						else {
							// the expected end of the lower line is not there so the lower line must be shorter
							y = y2s;	// put the y back to the lower line start and go pixel by pixel to find the end
							y2EndSearching = true;
						}
					}
				}
			}
			if (y1StartFound && !y1EndFound) {
				y1e = ymax;
				TFT_drawFastVLine(cx + x, cy + y1s, y1e - y1s + 1, color);
			}
			else if (y2StartFound && y2EndSearching) {
				// we found start of lower line but we are still searching for the end
				// which we haven't found in the loop so the last pixel in a column must be the end
				TFT_drawFastVLine(cx + x, cy + y2s, ymax - y2s + 1, color);
			}
		}
	}
}
*/

//----------------------------------------------------------------------------------------------
void drawPolygon(int cx, int cy, int sides, int diameter, color_t color, uint8_t fill, int deg)
{
  sides = (sides > 2? sides : 3);		// This ensures the minimum side number is 3.
  int Xpoints[sides], Ypoints[sides];	// Set the arrays based on the number of sides entered
  int rads = 360 / sides;				// This equally spaces the points.

  for (int idx = 0; idx < sides; idx++) {
    Xpoints[idx] = cx + sin((float)(idx*rads + deg) * deg_to_rad) * diameter;
    Ypoints[idx] = cy + cos((float)(idx*rads + deg) * deg_to_rad) * diameter;
  }

  for(int idx = 0; idx < sides; idx++)	// draws the polygon on the screen.
  {
    if( (idx+1) < sides)
    	TFT_drawLine(Xpoints[idx],Ypoints[idx],Xpoints[idx+1],Ypoints[idx+1], color); // draw the lines
    else
    	TFT_drawLine(Xpoints[idx],Ypoints[idx],Xpoints[0],Ypoints[0], color); // finishes the last line to close up the polygon.
  }
  if(fill)
    for(int idx = 0; idx < sides; idx++)
    {
      if((idx+1) < sides)
    	  TFT_fillTriangle(cx,cy,Xpoints[idx],Ypoints[idx],Xpoints[idx+1],Ypoints[idx+1], color);
      else
    	  TFT_fillTriangle(cx,cy,Xpoints[idx],Ypoints[idx],Xpoints[0],Ypoints[0], color);
    }
}

// Similar to the Polygon function.
//----------------------------------------------------------------------------------
void drawStar(int cx, int cy, int diameter, color_t color, bool fill, float factor)
{
  factor = constrain(factor, 1.0, 4.0);
  uint8_t sides = 5;
  uint8_t rads = 360 / sides;

  int Xpoints_O[sides], Ypoints_O[sides], Xpoints_I[sides], Ypoints_I[sides];//Xpoints_T[5], Ypoints_T[5];

  for(int idx = 0; idx < sides; idx++)
  {
	  // makes the outer points
    Xpoints_O[idx] = cx + sin((float)(idx*rads + 72) * deg_to_rad) * diameter;
    Ypoints_O[idx] = cy + cos((float)(idx*rads + 72) * deg_to_rad) * diameter;
    // makes the inner points
    Xpoints_I[idx] = cx + sin((float)(idx*rads + 36) * deg_to_rad) * ((float)(diameter)/factor);
    // 36 is half of 72, and this will allow the inner and outer points to line up like a triangle.
    Ypoints_I[idx] = cy + cos((float)(idx*rads + 36) * deg_to_rad) * ((float)(diameter)/factor);
  }

  for(int idx = 0; idx < sides; idx++)
  {
	if((idx+1) < sides)
	{
	  if(fill) // this part below should be self explanatory. It fills in the star.
	  {
		  TFT_fillTriangle(cx,cy,Xpoints_I[idx],Ypoints_I[idx],Xpoints_O[idx],Ypoints_O[idx], color);
		  TFT_fillTriangle(cx,cy,Xpoints_O[idx],Ypoints_O[idx],Xpoints_I[idx+1],Ypoints_I[idx+1], color);
	  }
	  else
	  {
		  TFT_drawLine(Xpoints_O[idx],Ypoints_O[idx],Xpoints_I[idx+1],Ypoints_I[idx+1], color);
		  TFT_drawLine(Xpoints_I[idx],Ypoints_I[idx],Xpoints_O[idx],Ypoints_O[idx], color);
	  }
	}
    else
    {
	  if(fill)
	  {
		  TFT_fillTriangle(cx,cy,Xpoints_I[0],Ypoints_I[0],Xpoints_O[idx],Ypoints_O[idx], color);
		  TFT_fillTriangle(cx,cy,Xpoints_O[idx],Ypoints_O[idx],Xpoints_I[idx],Ypoints_I[idx], color);
	  }
	  else
	  {
		  TFT_drawLine(Xpoints_O[idx],Ypoints_O[idx],Xpoints_I[idx],Ypoints_I[idx], color);
		  TFT_drawLine(Xpoints_I[0],Ypoints_I[0],Xpoints_O[idx],Ypoints_O[idx], color);
	  }
    }
  }
}


// ================ Font and string functions ==================================

// return max width of the proportional font
//--------------------------------
static uint8_t getMaxWidth(void) {
  uint16_t tempPtr = 4; // point at first char data
  uint8_t cc,cw,ch,w = 0;
  do
  {
    cc = cfont.font[tempPtr++];
    tempPtr++;
    cw = cfont.font[tempPtr++];
    ch = cfont.font[tempPtr++];
    tempPtr += 2;
    if (cc != 0xFF) {
      if (cw != 0) {
        if (cw > w) w = cw;
        // packed bits
        tempPtr += (((cw * ch)-1) / 8) + 1;
      }
    }
  } while (cc != 0xFF);

  return w;
}

//---------------------------------------------------
void TFT_setFont(uint8_t font, const char *font_file)
{
  cfont.font = NULL;

  if (font == FONT_7SEG) {
    cfont.bitmap = 2;
    cfont.x_size = 24;
    cfont.y_size = 6;
    cfont.offset = 0;
    cfont.color  = _fg;
  }
  else {
	  if (font == USER_FONT) cfont.font = tft_DefaultFont;
	  else if (font == DEJAVU18_FONT) cfont.font = tft_Dejavu18;
	  else if (font == DEJAVU24_FONT) cfont.font = tft_Dejavu24;
	  else if (font == UBUNTU16_FONT) cfont.font = tft_Ubuntu16;
	  else if (font == COMIC24_FONT) cfont.font = tft_Comic24;
	  else if (font == MINYA24_FONT) cfont.font = tft_minya24;
	  else if (font == TOONEY32_FONT) cfont.font = tft_tooney32;
	  else cfont.font = tft_DefaultFont;

	  cfont.bitmap = 1;
	  cfont.x_size = cfont.font[0];
	  cfont.y_size = cfont.font[1];
	  cfont.offset = cfont.font[2];
	  if (cfont.x_size != 0) cfont.numchars = cfont.font[3];
	  else cfont.numchars = getMaxWidth();
  }
}

// private method to return the Glyph data for an individual character in the proportional font
//--------------------------------
static int getCharPtr(uint8_t c) {
  uint16_t tempPtr = 4; // point at first char data

  do {
    fontChar.charCode = cfont.font[tempPtr++];
    fontChar.adjYOffset = cfont.font[tempPtr++];
    fontChar.width = cfont.font[tempPtr++];
    fontChar.height = cfont.font[tempPtr++];
    fontChar.xOffset = cfont.font[tempPtr++];
    fontChar.xOffset = fontChar.xOffset < 0x80 ? fontChar.xOffset : (0x100 - fontChar.xOffset);
    fontChar.xDelta = cfont.font[tempPtr++];

    if (c != fontChar.charCode && fontChar.charCode != 0xFF) {
      if (fontChar.width != 0) {
        // packed bits
        tempPtr += (((fontChar.width * fontChar.height)-1) / 8) + 1;
      }
    }
  } while (c != fontChar.charCode && fontChar.charCode != 0xFF);

  fontChar.dataPtr = tempPtr;
  if (c == fontChar.charCode) {
    if (_forceFixed > 0) {
      // fix width & offset for forced fixed width
      fontChar.xDelta = cfont.numchars;
      fontChar.xOffset = (fontChar.xDelta - fontChar.width) / 2;
    }
  }

  if (fontChar.charCode != 0xFF) return 1;
  else return 0;
}

// print rotated proportional character
// character is already in fontChar
//--------------------------------------------------------------
static int rotatePropChar(int x, int y, int offset) {
  uint8_t ch = 0;
  double radian = rotation * 0.0175;
  float cos_radian = cos(radian);
  float sin_radian = sin(radian);

  uint8_t mask = 0x80;
  disp_select();
  for (int j=0; j < fontChar.height; j++) {
    for (int i=0; i < fontChar.width; i++) {
      if (((i + (j*fontChar.width)) % 8) == 0) {
        mask = 0x80;
        ch = cfont.font[fontChar.dataPtr++];
      }

      int newX = (int)(x + (((offset + i) * cos_radian) - ((j+fontChar.adjYOffset)*sin_radian)));
      int newY = (int)(y + (((j+fontChar.adjYOffset) * cos_radian) + ((offset + i) * sin_radian)));

      if ((ch & mask) != 0) TFT_drawPixel(newX,newY,_fg, 0);
      else if (!_transparent) TFT_drawPixel(newX,newY,_bg, 0);

      mask >>= 1;
    }
  }
  disp_deselect();

  return fontChar.xDelta+1;
}

// print non-rotated proportional character
// character is already in fontChar
//---------------------------------------------------------
static int printProportionalChar(int x, int y) {
  uint8_t i,j,ch=0;
  uint16_t cx,cy;

  // fill background if not transparent background
  if (!_transparent) {
    TFT_fillRect(x, y, fontChar.xDelta+1, cfont.y_size, _bg);
  }

  // draw Glyph
  uint8_t mask = 0x80;
  disp_select();
  for (j=0; j < fontChar.height; j++) {
    for (i=0; i < fontChar.width; i++) {
      if (((i + (j*fontChar.width)) % 8) == 0) {
        mask = 0x80;
        ch = cfont.font[fontChar.dataPtr++];
      }

      if ((ch & mask) !=0) {
        cx = (uint16_t)(x+fontChar.xOffset+i);
        cy = (uint16_t)(y+j+fontChar.adjYOffset);
        TFT_drawPixel(cx, cy, _fg, 0);
      }
      mask >>= 1;
    }
  }
  disp_deselect();

  return fontChar.xDelta;
}

// non-rotated fixed width character
//----------------------------------------------
static void printChar(uint8_t c, int x, int y) {
  uint8_t i,j,ch,fz,mask;
  uint16_t k,temp,cx,cy;

  // fz = bytes per char row
  fz = cfont.x_size/8;
  if (cfont.x_size % 8) fz++;

  // get char address
  temp = ((c-cfont.offset)*((fz)*cfont.y_size))+4;

  // fill background if not transparent background
  if (!_transparent) {
    TFT_fillRect(x, y, cfont.x_size, cfont.y_size, _bg);
  }

  disp_select();
  for (j=0; j<cfont.y_size; j++) {
    for (k=0; k < fz; k++) {
      ch = cfont.font[temp+k];
      mask=0x80;
      for (i=0; i<8; i++) {
        if ((ch & mask) !=0) {
          cx = (uint16_t)(x+i+(k*8));
          cy = (uint16_t)(y+j);
          TFT_drawPixel(cx, cy, _fg, 0);
        }
        mask >>= 1;
      }
    }
    temp += (fz);
  }
  disp_deselect();
}

// rotated fixed width character
//--------------------------------------------------------
static void rotateChar(uint8_t c, int x, int y, int pos) {
  uint8_t i,j,ch,fz,mask;
  uint16_t temp;
  int newx,newy;
  double radian = rotation*0.0175;
  float cos_radian = cos(radian);
  float sin_radian = sin(radian);
  int zz;

  if( cfont.x_size < 8 ) fz = cfont.x_size;
  else fz = cfont.x_size/8;
  temp=((c-cfont.offset)*((fz)*cfont.y_size))+4;

  disp_select();
  for (j=0; j<cfont.y_size; j++) {
    for (zz=0; zz<(fz); zz++) {
      ch = cfont.font[temp+zz];
      mask = 0x80;
      for (i=0; i<8; i++) {
        newx=(int)(x+(((i+(zz*8)+(pos*cfont.x_size))*cos_radian)-((j)*sin_radian)));
        newy=(int)(y+(((j)*cos_radian)+((i+(zz*8)+(pos*cfont.x_size))*sin_radian)));

        if ((ch & mask) != 0) TFT_drawPixel(newx,newy,_fg, 0);
        else if (!_transparent) TFT_drawPixel(newx,newy,_bg, 0);
        mask >>= 1;
      }
    }
    temp+=(fz);
  }
  disp_deselect();
  // calculate x,y for the next char
  TFT_X = (int)(x + ((pos+1) * cfont.x_size * cos_radian));
  TFT_Y = (int)(y + ((pos+1) * cfont.x_size * sin_radian));
}

// returns the string width in pixels. Useful for positions strings on the screen.
//-----------------------------
int getStringWidth(char* str) {

  // is it 7-segment font?
  if (cfont.bitmap == 2) return ((2 * (2 * cfont.y_size + 1)) + cfont.x_size) * strlen(str);

  // is it a fixed width font?
  if (cfont.x_size != 0) return strlen(str) * cfont.x_size;
  else {
    // calculate the string width
    char* tempStrptr = str;
    int strWidth = 0;
    while (*tempStrptr != 0) {
      if (getCharPtr(*tempStrptr++)) strWidth += (fontChar.xDelta + 1);
    }
    return strWidth;
  }
}

//==============================================================================
/**
 * bit-encoded bar position of all digits' bcd segments
 *
 *                   6
 * 		  +-----+
 * 		3 |  .	| 2
 * 		  +--5--+
 * 		1 |  .	| 0
 * 		  +--.--+
 * 		     4
 */
static const uint16_t font_bcd[] = {
  0x200, // 0010 0000 0000  // -
  0x080, // 0000 1000 0000  // .
  0x06C, // 0100 0110 1100  // /, degree
  0x05f, // 0000 0101 1111, // 0
  0x005, // 0000 0000 0101, // 1
  0x076, // 0000 0111 0110, // 2
  0x075, // 0000 0111 0101, // 3
  0x02d, // 0000 0010 1101, // 4
  0x079, // 0000 0111 1001, // 5
  0x07b, // 0000 0111 1011, // 6
  0x045, // 0000 0100 0101, // 7
  0x07f, // 0000 0111 1111, // 8
  0x07d, // 0000 0111 1101  // 9
  0x900  // 1001 0000 0000  // :
};

//-------------------------------------------------------------------------------
static void barVert(int16_t x, int16_t y, int16_t w, int16_t l, color_t color) {
  TFT_fillTriangle(x+1, y+2*w, x+w, y+w+1, x+2*w-1, y+2*w, color);
  TFT_fillTriangle(x+1, y+2*w+l+1, x+w, y+3*w+l, x+2*w-1, y+2*w+l+1, color);
  TFT_fillRect(x, y+2*w+1, 2*w+1, l, color);
  if ((cfont.offset) && compare_colors(color, _bg)) {
    TFT_drawTriangle(x+1, y+2*w, x+w, y+w+1, x+2*w-1, y+2*w, cfont.color);
    TFT_drawTriangle(x+1, y+2*w+l+1, x+w, y+3*w+l, x+2*w-1, y+2*w+l+1, cfont.color);
    TFT_drawRect(x, y+2*w+1, 2*w+1, l, cfont.color);
  }
}

//------------------------------------------------------------------------------
static void barHor(int16_t x, int16_t y, int16_t w, int16_t l, color_t color) {
  TFT_fillTriangle(x+2*w, y+2*w-1, x+w+1, y+w, x+2*w, y+1, color);
  TFT_fillTriangle(x+2*w+l+1, y+2*w-1, x+3*w+l, y+w, x+2*w+l+1, y+1, color);
  TFT_fillRect(x+2*w+1, y, l, 2*w+1, color);
  if ((cfont.offset) && compare_colors(color, _bg)) {
    TFT_drawTriangle(x+2*w, y+2*w-1, x+w+1, y+w, x+2*w, y+1, cfont.color);
    TFT_drawTriangle(x+2*w+l+1, y+2*w-1, x+3*w+l, y+w, x+2*w+l+1, y+1, cfont.color);
    TFT_drawRect(x+2*w+1, y, l, 2*w+1, cfont.color);
  }
}

//------------------------------------------------------------------------------------------------
static void TFT_draw7seg(int16_t x, int16_t y, int8_t num, int16_t w, int16_t l, color_t color) {
  /* TODO: clipping */
  if (num < 0x2D || num > 0x3A) return;

  int16_t c = font_bcd[num-0x2D];
  int16_t d = 2*w+l+1;

  //if (!_transparent) TFT_fillRect(x, y, (2 * (2 * w + 1)) + l, (3 * (2 * w + 1)) + (2 * l), _bg);

  if (!(c & 0x001)) barVert(x+d, y+d, w, l, _bg);
  if (!(c & 0x002)) barVert(x,   y+d, w, l, _bg);
  if (!(c & 0x004)) barVert(x+d, y, w, l, _bg);
  if (!(c & 0x008)) barVert(x,   y, w, l, _bg);
  if (!(c & 0x010)) barHor(x, y+2*d, w, l, _bg);
  if (!(c & 0x020)) barHor(x, y+d, w, l, _bg);
  if (!(c & 0x040)) barHor(x, y, w, l, _bg);

  //if (!(c & 0x080)) TFT_fillRect(x+(d/2), y+2*d, 2*w+1, 2*w+1, _bg);
  if (!(c & 0x100)) TFT_fillRect(x+(d/2), y+d+2*w+1, 2*w+1, l/2, _bg);
  if (!(c & 0x800)) TFT_fillRect(x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, _bg);
  //if (!(c & 0x200)) TFT_fillRect(x+2*w+1, y+d, l, 2*w+1, _bg);

  if (c & 0x001) barVert(x+d, y+d, w, l, color);               // down right
  if (c & 0x002) barVert(x,   y+d, w, l, color);               // down left
  if (c & 0x004) barVert(x+d, y, w, l, color);                 // up right
  if (c & 0x008) barVert(x,   y, w, l, color);                 // up left
  if (c & 0x010) barHor(x, y+2*d, w, l, color);                // down
  if (c & 0x020) barHor(x, y+d, w, l, color);                  // middle
  if (c & 0x040) barHor(x, y, w, l, color);                    // up

  if (c & 0x080) {
    TFT_fillRect(x+(d/2), y+2*d, 2*w+1, 2*w+1, color);         // low point
    if (cfont.offset) TFT_drawRect(x+(d/2), y+2*d, 2*w+1, 2*w+1, cfont.color);
  }
  if (c & 0x100) {
    TFT_fillRect(x+(d/2), y+d+2*w+1, 2*w+1, l/2, color);       // down middle point
    if (cfont.offset) TFT_drawRect(x+(d/2), y+d+2*w+1, 2*w+1, l/2, cfont.color);
  }
  if (c & 0x800) {
    TFT_fillRect(x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, color); // up middle point
    if (cfont.offset) TFT_drawRect(x+(d/2), y+(2*w)+1+(l/2), 2*w+1, l/2, cfont.color);
  }
  if (c & 0x200) {
    TFT_fillRect(x+2*w+1, y+d, l, 2*w+1, color);               // middle, minus
    if (cfont.offset) TFT_drawRect(x+2*w+1, y+d, l, 2*w+1, cfont.color);
  }
}
//==============================================================================


//--------------------------------------
void TFT_print(char *st, int x, int y) {
  int stl, i, tmpw, tmph, fh;
  uint8_t ch;

  if (cfont.bitmap == 0) return; // wrong font selected

  // for rotated string x cannot be RIGHT or CENTER
  if ((rotation != 0) && ((x < -2) || (y < -2))) return;

  stl = strlen(st); // number of characters in string to print

  // set CENTER or RIGHT possition
  tmpw = getStringWidth(st);
  fh = cfont.y_size; // font height
  if ((cfont.x_size != 0) && (cfont.bitmap == 2)) {
    // 7-segment font
    fh = (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size);  // character height
  }

  if (x==RIGHT) x = dispWin.x2 - tmpw - 1;
  if (x==CENTER) x = (dispWin.x2 - tmpw - 1)/2;
  if (y==BOTTOM) y = dispWin.y2 - fh - 1;
  if (y==CENTER) y = (dispWin.y2 - (fh/2) - 1)/2;
  if (x < dispWin.x1) x = dispWin.x1;
  if (y < dispWin.y1) y = dispWin.y1;

  TFT_X = x;
  TFT_Y = y;
  int offset = TFT_OFFSET;


  tmph = cfont.y_size; // font height
  // for non-proportional fonts, char width is the same for all chars
  if (cfont.x_size != 0) {
    if (cfont.bitmap == 2) { // 7-segment font
      tmpw = (2 * (2 * cfont.y_size + 1)) + cfont.x_size;        // character width
      tmph = (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size);  // character height
    }
    else tmpw = cfont.x_size;
  }
  if ((TFT_Y + tmph - 1) > dispWin.y2) return;

  // adjust y position


  for (i=0; i<stl; i++) {
    ch = *st++; // get char

    if (cfont.x_size == 0) {
      // for proportional font get char width
      if (getCharPtr(ch)) tmpw = fontChar.xDelta;
    }

    if (ch == 0x0D) { // === '\r', erase to eol ====
      if ((!_transparent) && (rotation==0)) TFT_fillRect(TFT_X, TFT_Y,  dispWin.x2+1-TFT_X, tmph, _bg);
    }

    else if (ch == 0x0A) { // ==== '\n', new line ====
      if (cfont.bitmap == 1) {
        TFT_Y += tmph;
        if (TFT_Y > (dispWin.y2-tmph)) break;
        TFT_X = dispWin.x1;
      }
    }

    else { // ==== other characters ====
      // check if character can be displayed in the current line
      if ((TFT_X+tmpw) > (dispWin.x2+1)) {
        if (_wrap == 0) break;
        TFT_Y += tmph;
        if (TFT_Y > (dispWin.y2-tmph)) break;
        TFT_X = dispWin.x1;
      }

      // Let's print the character
      if (cfont.x_size == 0) {
        // == proportional font
        if (rotation==0) {
          TFT_X += printProportionalChar(TFT_X, TFT_Y)+1;
        }
        else {
          offset += rotatePropChar(x, y, offset);
          TFT_OFFSET = offset;
        }
      }
      // == fixed font
      else {
        if (cfont.bitmap == 1) {
          if ((ch < cfont.offset) || ((ch-cfont.offset) > cfont.numchars)) ch = cfont.offset;
          if (rotation==0) {
            printChar(ch, TFT_X, TFT_Y);
            TFT_X += tmpw;
          }
          else rotateChar(ch, x, y, i);
        }
        else if (cfont.bitmap == 2) { // 7-seg font
          TFT_draw7seg(TFT_X, TFT_Y, ch, cfont.y_size, cfont.x_size, _fg);
          TFT_X += (tmpw + 2);
        }
      }
    }
  }
  TFT_OFFSET = 0;
}


// ================ Service functions ==========================================

// Change the screen rotation.
// Input: m new rotation value (0 to 3)
//-------------------------------
void TFT_setRotation(uint8_t m) {
	uint8_t rotation = m & 3; // can't be higher than 3
	uint8_t send = 1;
	uint8_t madctl = 0;

	if (m > 3) madctl = (m & 0xF8); // for testing, manually set MADCTL register
	else {
		orientation = m;
		if (tft_disp_type == DISP_TYPE_ILI9488) {
			if ((rotation & 1)) {
				_width  = ILI9488_HEIGHT;
				_height = ILI9488_WIDTH;
			}
			else {
				_width  = ILI9488_WIDTH;
				_height = ILI9488_HEIGHT;
			}
		}
		else if (tft_disp_type == DISP_TYPE_ILI9341){
			if ((rotation & 1)) {
				_width  = ILI9341_HEIGHT;
				_height = ILI9341_WIDTH;
			}
			else {
				_width  = ILI9341_WIDTH;
				_height = ILI9341_HEIGHT;
			}
		}
		else send = 0;
		switch (rotation) {
		  case PORTRAIT:
			madctl = (MADCTL_MX | MADCTL_BGR);
			break;
		  case LANDSCAPE:
			madctl = (MADCTL_MV | MADCTL_BGR);
			break;
		  case PORTRAIT_FLIP:
			madctl = (MADCTL_MY | MADCTL_BGR);
			break;
		  case LANDSCAPE_FLIP:
			madctl = (MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
			break;
		}
	}
	if (send) {
		if (disp_select() == ESP_OK) {
			disp_spi_transfer_cmd_data(TFT_MADCTL, &madctl, 1);
			disp_deselect();
		}
	}

	dispWin.x1 = 0;
	dispWin.y1 = 0;
	dispWin.x2 = _width-1;
	dispWin.y2 = _height-1;
}

// Send the command to invert all of the colors.
// Input: i 0 to disable inversion; non-zero to enable inversion
//------------------------------------------
void TFT_invertDisplay(const uint8_t mode) {
  if ( mode == INVERT_ON ) disp_spi_transfer_cmd(TFT_INVONN);
  else disp_spi_transfer_cmd(TFT_INVOFF);
}

//-----------------------------------------------------------
color_t HSBtoRGB(float _hue, float _sat, float _brightness) {
 float red = 0.0;
 float green = 0.0;
 float blue = 0.0;

 if (_sat == 0.0) {
   red = _brightness;
   green = _brightness;
   blue = _brightness;
 } else {
   if (_hue == 360.0) {
     _hue = 0;
   }

   int slice = (int)(_hue / 60.0);
   float hue_frac = (_hue / 60.0) - slice;

   float aa = _brightness * (1.0 - _sat);
   float bb = _brightness * (1.0 - _sat * hue_frac);
   float cc = _brightness * (1.0 - _sat * (1.0 - hue_frac));

   switch(slice) {
     case 0:
         red = _brightness;
         green = cc;
         blue = aa;
         break;
     case 1:
         red = bb;
         green = _brightness;
         blue = aa;
         break;
     case 2:
         red = aa;
         green = _brightness;
         blue = cc;
         break;
     case 3:
         red = aa;
         green = bb;
         blue = _brightness;
         break;
     case 4:
         red = cc;
         green = aa;
         blue = _brightness;
         break;
     case 5:
         red = _brightness;
         green = aa;
         blue = bb;
         break;
     default:
         red = 0.0;
         green = 0.0;
         blue = 0.0;
         break;
   }
 }

 color_t color;
 color.r = ((uint8_t)(red * 255.0)) & 0xFC;
 color.g = ((uint8_t)(green * 255.0)) & 0xFC;
 color.b = ((uint8_t)(blue * 255.0)) & 0xFC;

 return color;
}
//---------------------------------------------------------------------
void tft_setclipwin(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	dispWin.x1 = x1;
	dispWin.y1 = y1;
	dispWin.x2 = x2;
	dispWin.y2 = y2;

	if (dispWin.x2 >= _width) dispWin.x2 = _width-1;
	if (dispWin.y2 >= _height) dispWin.y2 = _height-1;
	if (dispWin.x1 > dispWin.x2) dispWin.x1 = dispWin.x2;
	if (dispWin.y1 > dispWin.y2) dispWin.y1 = dispWin.y2;
}

//---------------------
void tft_resetclipwin()
{
	dispWin.x2 = _width-1;
	dispWin.y2 = _height-1;
	dispWin.x1 = 0;
	dispWin.y1 = 0;
}

//---------------------------------------------------------------------
void set_font_atrib(uint8_t l, uint8_t w, int offset, color_t color) {
	cfont.x_size = l;
	cfont.y_size = w;
	cfont.offset = offset;
	cfont.color  = color;
}

//------------------------------------------
int tft_getfontsize(int *width, int* height)
{
  if (cfont.bitmap == 1) {
    if (cfont.x_size != 0) *width = cfont.x_size;
    else *width = getMaxWidth();
    *height = cfont.y_size;
  }
  else if (cfont.bitmap == 2) {
    *width = (2 * (2 * cfont.y_size + 1)) + cfont.x_size;
    *height = (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size);
  }
  else {
    *width = 0;
    *height = 0;
    return 0;
  }
  return 1;
}

//---------------------
int tft_getfontheight()
{
  if (cfont.bitmap == 1) {
	// Bitmap font
    return cfont.y_size;
  }
  else if (cfont.bitmap == 2) {
	// 7-segment font
    return (3 * (2 * cfont.y_size + 1)) + (2 * cfont.x_size);
  }
  return 0;
}



// ================ JPG SUPPORT ================================================
// User defined device identifier
typedef struct {
	FILE *fhndl;		// File handler for input function
    uint16_t x;			// image top left point X position
    uint16_t y;			// image top left point Y position
    uint8_t *membuff;	// memory buffer containing the image
    uint32_t bufsize;	// size of the memory buffer
    uint32_t bufptr;	// memory buffer current possition
} JPGIODEV;


// User defined call-back function to input JPEG data from file
//---------------------
static UINT tjd_input (
	JDEC* jd,		// Decompression object
	BYTE* buff,		// Pointer to the read buffer (NULL:skip)
	UINT nd			// Number of bytes to read/skip from input stream
)
{
	int rb = 0;
	// Device identifier for the session (5th argument of jd_prepare function)
	JPGIODEV *dev = (JPGIODEV*)jd->device;

	if (buff) {	// Read nd bytes from the input strem
		rb = fread(buff, 1, nd, dev->fhndl);
		return rb;	// Returns actual number of bytes read
	}
	else {	// Remove nd bytes from the input stream
		if (fseek(dev->fhndl, nd, SEEK_CUR) >= 0) return nd;
		else return 0;
	}
}

// User defined call-back function to input JPEG data from memory buffer
//-------------------------
static UINT tjd_buf_input (
	JDEC* jd,		// Decompression object
	BYTE* buff,		// Pointer to the read buffer (NULL:skip)
	UINT nd			// Number of bytes to read/skip from input stream
)
{
	// Device identifier for the session (5th argument of jd_prepare function)
	JPGIODEV *dev = (JPGIODEV*)jd->device;
	if (!dev->membuff) return 0;
	if (dev->bufptr >= (dev->bufsize + 2)) return 0; // end of stream

	if ((dev->bufptr + nd) > (dev->bufsize + 2)) nd = (dev->bufsize + 2) - dev->bufptr;

	if (buff) {	// Read nd bytes from the input strem
		memcpy(buff, dev->membuff + dev->bufptr, nd);
		dev->bufptr += nd;
		return nd;	// Returns number of bytes read
	}
	else {	// Remove nd bytes from the input stream
		dev->bufptr += nd;
		return nd;
	}
}

// User defined call-back function to output RGB bitmap to display device
//----------------------
static UINT tjd_output (
	JDEC* jd,		// Decompression object of current session
	void* bitmap,	// Bitmap data to be output
	JRECT* rect		// Rectangular region to output
)
{
	// Device identifier for the session (5th argument of jd_prepare function)
	JPGIODEV *dev = (JPGIODEV*)jd->device;

	// ** Put the rectangular into the display device **
	uint16_t x;
	uint16_t y;
	BYTE *src = (BYTE*)bitmap;

	uint16_t left = rect->left + dev->x;
	uint16_t top = rect->top + dev->y;
	uint16_t right = rect->right + dev->x;
	uint16_t bottom = rect->bottom + dev->y;
	uint8_t *dest = (uint8_t *)tft_line;

	if ((left >= _width) || (top >= _height)) return 1;	// out of screen area, return

	uint32_t len = ((right-left+1) * (bottom-top+1));		// calculate length of data

	if ((len > 0) && (len <= (TFT_LINEBUF_MAX_SIZE))) {
		uint16_t dright = 0;
		uint16_t dbottom = 0;
	    for (y = top; y <= bottom; y++) {
		    for (x = left; x <= right; x++) {
		    	// Clip to display area
		    	if ((x < _width) && (y < _height)) {
		    		*dest++ = *src++;
		    		*dest++ = *src++;
		    		*dest++ = *src++;
		    		if (dright < x) dright = x;
		    		if (dbottom < y) dbottom = y;
		    	}
		    	else src += 3;
		    }
	    }
		len = ((dright-left+1) * (dbottom-top+1));		// calculate length of data

		disp_deselect();
		send_data(left, top, dright, dbottom, len, tft_line);
	}
	else {
		printf("max data size exceded: %d (%d,%d,%d,%d)\r\n", len, left,top,right,bottom);
		return 0;  // stop decompression
	}

	return 1;	// Continue to decompression
}

// tft.jpgimage(X, Y, scale, file_name, buf, size]
//===============================================================================
void tft_jpg_image(int x, int y, int ascale, char *fname, uint8_t *buf, int size)
{
	JPGIODEV dev;
    struct stat sb;
    uint8_t dbg = 0;
    int maxscale = 3;
	BYTE scale = 0;

    if (!tft_line) {
    	if (dbg) printf("Error: Line buffer not allocated\r\n");
        return;
    }

    if (fname == NULL) {
    	// image from buffer
    	dev.fhndl = NULL;
        dev.membuff = buf;
        dev.bufsize = size;
        dev.bufptr = 0;
    }
    else {
    	// image from file
        dev.membuff = NULL;
        dev.bufsize = 0;

        if (stat(fname, &sb) != 0) {
        	if (dbg) printf("File error: %ss\r\n", strerror(errno));
        }

        dev.fhndl = fopen(fname, "r");
        if (!dev.fhndl) {
        	if (dbg) printf("Error opening file: %s\r\n", strerror(errno));
        }
    }

	char *work;				// Pointer to the working buffer (must be 4-byte aligned)
	UINT sz_work = 3800;	// Size of the working buffer (must be power of 2)
	JDEC jd;				// Decompression object (70 bytes)
	JRESULT rc;
	uint8_t radj = 0;
	uint8_t badj = 0;

	if ((x < 0) && (x != CENTER) && (x != RIGHT)) x = 0;
	if ((y < 0) && (y != CENTER) && (y != BOTTOM)) y = 0;
	if (x > (_width-5)) x = _width - 5;
	if (y > (_height-5)) y = _height - 5;

	work = malloc(sz_work);
	if (work) {
		if (ascale >= 0) {
			scale = ascale;
			maxscale = 0;
		}
		if (dev.membuff) rc = jd_prepare(&jd, tjd_buf_input, (void *)work, sz_work, &dev);
		else rc = jd_prepare(&jd, tjd_input, (void *)work, sz_work, &dev);
		if (rc == JDR_OK) {
			if (x == CENTER) {
				x = _width - (jd.width >> scale);
				if (x < 0) {
					if (maxscale) {
						for (scale = 0; scale <= maxscale; scale++) {
							if (((jd.width >> scale) <= (_width)) && ((jd.height >> scale) <= (_height))) break;
							if (scale == maxscale) break;
						}
						x = _width - (jd.width >> scale);
						if (x < 0) x = 0;
						else x >>= 1;
						maxscale = 0;
					}
					else x = 0;
				}
				else x >>= 1;
			}
			if (y == CENTER) {
				y = _height - (jd.height >> scale);
				if (y < 0) {
					if (maxscale) {
						for (scale = 0; scale <= maxscale; scale++) {
							if (((jd.width >> scale) <= (_width)) && ((jd.height >> scale) <= (_height))) break;
							if (scale == maxscale) break;
						}
						y = _height - (jd.height >> scale);
						if (y < 0) y = 0;
						else y >>= 1;
						maxscale = 0;
					}
					else y = 0;
				}
				else y >>= 1;
			}
			if (x == RIGHT) {
				x = 0;
				radj = 1;
			}
			if (y == BOTTOM) {
				y = 0;
				badj = 1;
			}
			// Determine scale factor
			if (maxscale) {
				for (scale = 0; scale <= maxscale; scale++) {
					if (((jd.width >> scale) <= (_width-x)) && ((jd.height >> scale) <= (_height-y))) break;
					if (scale == maxscale) break;
				}
			}
			if (dbg) printf("Image dimensions: %dx%d, scale: %d, bytes used: %d\r\n", jd.width, jd.height, scale, jd.sz_pool);

			if (radj) {
				x = _width - (jd.width >> scale);
				if (x < 0) x = 0;
			}
			if (badj) {
				y = _height - (jd.height >> scale);
				if (y < 0) y = 0;
			}
			dev.x = x;
			dev.y = y;
			// Start to decompress the JPEG file

			rc = jd_decomp(&jd, tjd_output, scale);
			disp_deselect();
			if (rc != JDR_OK) {
				if (dbg) printf("jpg decompression error %d\r\n", rc);
			}
		}
		else {
			if (dbg) printf("jpg prepare error %d\r\n", rc);
		}

		free(work);  // free work buffer
	}
	else {
		if (dbg) printf("work buffer allocation error\r\n");
	}

    if (dev.fhndl) fclose(dev.fhndl);  // close input file
}

//=====================================================================
int tft_bmp_image(int x, int y, char *fname, uint8_t *imgbuf, int size)
{
	vTaskDelay(500 / portTICK_RATE_MS);
	FILE *fhndl = NULL;
	struct stat sb;
	uint32_t xrd = 0;
	uint8_t err = 9;
	uint8_t tmpc;
	int i;
	uint8_t use_trans = tft_use_trans;

    if (!tft_line) {
	    printf("Line buffer not allocated\r\n");
	    return -1;
    }

    uint8_t *buf = (uint8_t *)tft_line;

    if (fname) {
    	if (stat(fname, &sb) != 0) {
			printf("error opening file\r\n");
    		return -3;
    	}
		fhndl = fopen(fname, "r");
		if (!fhndl) {
			printf("error opening file\r\n");
			return -3;
		}

		xrd = fread(buf, 1, 54, fhndl);  // read header
    }
    else {
    	xrd = 0;
    	if ((imgbuf) && (size > 54)) {
    		memcpy(buf, imgbuf, 54);
    		xrd = 54;
    	}
    }
	if (xrd != 54) {
exithd:
		if (fhndl) fclose(fhndl);
		printf("Error reading header: %d\r\n", err);
		return -4;
	}

	uint16_t wtemp;
	uint32_t temp;
	uint32_t offset;
	uint32_t xsize;
	uint32_t ysize;

	// Check image header
	if ((buf[0] != 'B') || (buf[1] != 'M')) {err=1; goto exithd;}

	memcpy(&offset, buf+10, 4);
	memcpy(&temp, buf+14, 4);
	if (temp != 40)  {err=2; goto exithd;}
	memcpy(&wtemp, buf+26, 2);
	if (wtemp != 1)  {err=3; goto exithd;}
	memcpy(&wtemp, buf+28, 2);
	if (wtemp != 24)  {err=4; goto exithd;}
	memcpy(&temp, buf+30, 4);
	if (temp != 0)  {err=5; goto exithd;}

	memcpy(&xsize, buf+18, 4);
	memcpy(&ysize, buf+22, 4);

	// Adjust position
	if (x == CENTER) x = (_width - xsize) / 2;
	else if (x == RIGHT) x = (_width - xsize);
	if (x < 0) x = 0;

	if (y == CENTER) y = (_height - ysize) / 2;
	else if (y == BOTTOM) y = (_height - ysize);
	if (y < 0) y = 0;

	// Crop to display width
	int xend;
	if ((x+xsize) > _width) xend = _width-1;
	else xend = x+xsize-1;
	int disp_xsize = xend-x+1;
	if ((disp_xsize <= 1) || (y >= _height)) {
		printf("image out of screen.\r\n");
		goto exit;
	}

	// :TODO, When using DMA mode, BMP does not work well, force direct mode;
	tft_use_trans = 0;

	while (ysize > 0) {
		// Position at line start
		// ** BMP images are stored in file from LAST to FIRST line
		//    so we have to read from the end line first

		if (fhndl) {
			if (fseek(fhndl, offset+((ysize-1)*(xsize*3)), SEEK_SET) != 0) break;
		}
		else {
			if (offset+((ysize-1)*(xsize*3)) > (size-(xsize*3))) {
				printf("line offset error [line=%d]\r\n", ysize-1);
				break;
			}
			xrd = offset+((ysize-1)*(xsize*3));
		}

		// ** read one image line from file and send to display **
		// read only the part of image line which can be shown on screen
		if (fhndl) xrd = fread(buf, 1, disp_xsize*3, fhndl);  // read line from file
		else {
			if ((xrd+(disp_xsize*3)) > size) xrd = 0;
			else {
				memcpy(buf, imgbuf+xrd, disp_xsize*3);
				xrd = disp_xsize*3;
			}
		}
		if (xrd != (disp_xsize*3)) {
			printf("Error reading line: %d (%d)\r\n", y, xrd);
			break;
		}
		// Convert colors BGR-888 (BMP) -> RGB-888 (DISPLAY)
		for (i=0;i < xrd;i += 3) {
			tmpc = buf[i+2] & 0xfc;    // save R
			buf[i+2] = buf[i] & 0xfc;  // B -> R
			buf[i] = tmpc;             // R -> B
			buf[i+1] &= 0xfc;          // G
		}

		disp_deselect();
	    send_data(x, y, xend, y, disp_xsize, tft_line);

		y++;	// next image line
		if (y >= _height) break;
		ysize--;
	}
	disp_deselect();
	tft_use_trans = use_trans;  // restore mode

exit:
	if (fhndl) fclose(fhndl);

	return 0;
}


// ============= Touch panel functions =========================================

//-----------------------------------------------
static int tp_get_data(uint8_t type, int samples)
{
	int n, result, val = 0;
	uint32_t i = 0;
	uint32_t vbuf[18];
	uint32_t minval, maxval, dif;

    if (samples < 3) samples = 1;
    if (samples > 18) samples = 18;

    // one dummy read
    result = touch_get_data(type);

    // read data
	while (i < 10) {
    	minval = 5000;
    	maxval = 0;
		// get values
		for (n=0;n<samples;n++) {
		    result = touch_get_data(type);
			if (result < 0) break;

			vbuf[n] = result;
			if (result < minval) minval = result;
			if (result > maxval) maxval = result;
		}
		if (result < 0) break;
		dif = maxval - minval;
		if (dif < 40) break;
		i++;
    }
	if (result < 0) return -1;

	if (samples > 2) {
		// remove one min value
		for (n = 0; n < samples; n++) {
			if (vbuf[n] == minval) {
				vbuf[n] = 5000;
				break;
			}
		}
		// remove one max value
		for (n = 0; n < samples; n++) {
			if (vbuf[n] == maxval) {
				vbuf[n] = 5000;
				break;
			}
		}
		for (n = 0; n < samples; n++) {
			if (vbuf[n] < 5000) val += vbuf[n];
		}
		val /= (samples-2);
	}
	else val = vbuf[0];

    return val;
}

//=============================================
int tft_read_touch(int *x, int* y, uint8_t raw)
{
	int result = -1;
    int32_t X=0, Y=0, tmp;

    *x = 0;
    *y = 0;

    result = tp_get_data(0xB0, 3);
	if (result > 50)  {
		// tp pressed
		result = tp_get_data(0xD0, 10);
		if (result >= 0) {
			X = result;

			result = tp_get_data(0x90, 10);
			if (result >= 0) Y = result;
		}
	}

	if (result <= 50) return 0;

	if (raw) {
		*x = X;
		*y = Y;
		return result;
	}

	int xleft   = (tp_calx >> 16) & 0x3FFF;
	int xright  = tp_calx & 0x3FFF;
	int ytop    = (tp_caly >> 16) & 0x3FFF;
	int ybottom = tp_caly & 0x3FFF;

	int width = ILI9341_WIDTH;
	int height = ILI9341_HEIGHT;
	if (tft_disp_type == DISP_TYPE_ILI9488) {
		width = ILI9488_WIDTH;
		height = ILI9488_HEIGHT;
	}

	if (((xright - xleft) != 0) && ((ybottom - ytop) != 0)) {
		X = ((X - xleft) * height) / (xright - xleft);
		Y = ((Y - ytop) * width) / (ybottom - ytop);
	}
	else return 0;

	if (X < 0) X = 0;
	if (X > height-1) X = height-1;
	if (Y < 0) Y = 0;
	if (Y > width-1) Y = width-1;

	switch (orientation) {
		case PORTRAIT:
			tmp = X;
			X = width - Y - 1;
			Y = tmp;
			break;
		case PORTRAIT_FLIP:
			tmp = X;
			X = Y;
			Y = height - tmp - 1;
			break;
		case LANDSCAPE_FLIP:
			X = height - X - 1;
			Y = width - Y - 1;
			break;
	}

	*x = X;
	*y = Y;
	return result;
}

