#ifndef _LCD_h
#define _LCD_h
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

void lcdstart();
void lcdprintln(String str);
void lcdprint_hnh( String str, unsigned int row );
void lcdprint( String str, unsigned int row, unsigned int col);

#endif
