
#ifndef _TFT_H_
#define _TFT_H_

#include <stdlib.h>

uint8_t		orientation;
uint8_t		rotation;
uint8_t		_transparent;
uint16_t	_fg;
uint16_t	_bg;
uint8_t		_wrap;
uint8_t		_forceFixed;


void TFT_drawPixel(int16_t x, int16_t y, uint16_t color, uint8_t sel);
uint16_t TFT_readPixel(int16_t x, int16_t y);
void TFT_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void TFT_drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void TFT_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void TFT_fillScreen(uint16_t color);
void TFT_drawRect(uint16_t x1,uint16_t y1,uint16_t w,uint16_t h, uint16_t color);
void TFT_drawRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color);
void TFT_fillRoundRect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t color);
void TFT_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void TFT_drawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void TFT_fillTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void TFT_drawCircle(int16_t x, int16_t y, int radius, uint16_t color);
void TFT_fillCircle(int16_t x, int16_t y, int radius, uint16_t color);
void TFT_draw_ellipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, uint16_t color, uint8_t option);
void TFT_draw_filled_ellipse(uint16_t x0, uint16_t y0, uint16_t rx, uint16_t ry, uint16_t color, uint8_t option);
void drawPolygon(int cx, int cy, int sides, int diameter, uint16_t color, uint8_t fill, int deg);
void drawStar(int cx, int cy, int diameter, uint16_t color, bool fill, float factor);

void TFT_setFont(uint8_t font, const char *font_file);
void TFT_print(char *st, int x, int y);
void tft_setclipwin(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void tft_resetclipwin();
void set_font_atrib(uint8_t l, uint8_t w, int offset, uint16_t color);

void TFT_setRotation(uint8_t m);
void TFT_invertDisplay(const uint8_t mode);

uint16_t HSBtoRGB(float _hue, float _sat, float _brightness);

void tft_jpg_image(int x, int y, int maxscale, char *fname, uint8_t *buf, int size);

int tp_get_data(uint8_t type, int samples);

#endif
