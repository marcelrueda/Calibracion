/******************************************************************************
 SSD1306 OLED driver adapted for ESP32 IDF (ssd1306.c)                        *
                                                                              *
 Based on the original driver for CCS PIC C compiler by Simple Circuit.       *
 Reference: Adafruit Industries SSD1306 OLED driver and graphics library.     *
                                                                              *
 This adaptation is for I2C mode on ESP32 using ESP-IDF framework.            *
                                                                              *
 Original source: http://simple-circuit.com/                                  *
 Adafruit source: http://www.adafruit.com/category/63_98                      *
                                                                              *
 Adafruit invests time and resources providing this open-source code,         *
 please support Adafruit and open-source hardware by purchasing               *
 products from Adafruit!                                                      *
                                                                              *
 Written by Limor Fried/Ladyada for Adafruit Industries.                      *
 Adapted for ESP32 IDF by Carlos Andrés Vargas C.                             *
 Contact: carlos_vargas_c@outlook.com                                         *
 BSD license, check license.txt for more information.                         *
 All text above, and the splash screen must be included in any redistribution.*
*******************************************************************************/

//!     S0                    S127
//! PAG 0| | | | | | | | |....|   |
//! PAG 1| | | | | | | | |....|   |
//! PAG 2| | | | | | | | |....|   |
//! PAG 3| | | | | | | | |....|   |
//! PAG 4| | | | | | | | |....|   |
//! PAG 5| | | | | | | | |....|   |
//! PAG 6| | | | | | | | |....|   |
//! PAG 7| | | | | | | | |....|   |

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "ssd1306.h"
#include "esp_log.h"
#include "font.c"

// Variables globales
static uint8_t buffer[SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8]; // Buffer para el display
uint8_t _i2caddr;                                                // Dirección I2C
uint8_t _vccstate;                                               //
int wrap = 1;                                                    //

static inline uint8_t bit_test(uint8_t value, uint8_t bit)
{
    return (value >> bit) & 0x01;
}
// Send a command to the SSD1306
void ssd1306_command(HelperI2CDevice *dev, uint8_t c)
{
    uint8_t data[2] = {0x00, c}; // Control byte (Co = 0, D/C = 0) followed by the command
    helper_i2c_write(dev, data, sizeof(data));
}

// Initialize the SSD1306 display
esp_err_t SSD1306_Begin(HelperI2CDevice *dev, uint8_t vccstate, uint8_t i2caddr)
{
    _vccstate = vccstate;
    _i2caddr = i2caddr;

    // Rest of the SSD1306 initialization sequence
    ssd1306_command(dev, SSD1306_DISPLAYOFF);         // 0xAE
    ssd1306_command(dev, SSD1306_SETDISPLAYCLOCKDIV); // 0xD5
    ssd1306_command(dev, 0x80);                       // Suggested ratio 0x80
    ssd1306_command(dev, SSD1306_SETMULTIPLEX);       // 0xA8
    ssd1306_command(dev, SSD1306_LCDHEIGHT - 1);
    ssd1306_command(dev, SSD1306_SETDISPLAYOFFSET);   // 0xD3
    ssd1306_command(dev, 0x0);                        // No offset
    ssd1306_command(dev, SSD1306_SETSTARTLINE | 0x0); // Line #0
    ssd1306_command(dev, SSD1306_CHARGEPUMP);         // 0x8D
    if (vccstate == SSD1306_EXTERNALVCC)
    {
        ssd1306_command(dev, 0x10);
    }
    else
    {
        ssd1306_command(dev, 0x14);
    }
    ssd1306_command(dev, SSD1306_MEMORYMODE); // 0x20
    ssd1306_command(dev, 0x00);               // Horizontal addressing mode
    ssd1306_command(dev, SSD1306_SEGREMAP | 0x1);
    ssd1306_command(dev, SSD1306_COMSCANDEC);
    ssd1306_command(dev, SSD1306_SETCOMPINS); // 0xDA
    ssd1306_command(dev, 0x12);
    ssd1306_command(dev, SSD1306_SETCONTRAST); // 0x81
    if (vccstate == SSD1306_EXTERNALVCC)
    {
        ssd1306_command(dev, 0x9F);
    }
    else
    {
        ssd1306_command(dev, 0xCF);
    }
    ssd1306_command(dev, SSD1306_SETPRECHARGE); // 0xD9
    if (vccstate == SSD1306_EXTERNALVCC)
    {
        ssd1306_command(dev, 0x22);
    }
    else
    {
        ssd1306_command(dev, 0xF1);
    }
    ssd1306_command(dev, SSD1306_SETVCOMDETECT); // 0xDB
    ssd1306_command(dev, 0x40);
    ssd1306_command(dev, SSD1306_DISPLAYALLON_RESUME); // 0xA4
    ssd1306_command(dev, SSD1306_NORMALDISPLAY);       // 0xA6
    ssd1306_command(dev, SSD1306_DISPLAYON);           // Turn on the display

    return ESP_OK;
}

// Send the buffer to the display
void SSD1306_Display(HelperI2CDevice *dev)
{
    ssd1306_command(dev, SSD1306_COLUMNADDR);
    ssd1306_command(dev, 0);                    // Column start address
    ssd1306_command(dev, SSD1306_LCDWIDTH - 1); // Column end address

    ssd1306_command(dev, SSD1306_PAGEADDR);
    ssd1306_command(dev, 0); // Page start address
    ssd1306_command(dev, 7); // Page end address

    for (uint16_t i = 0; i < (SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8); i += 16)
    {
        uint8_t data[17];
        data[0] = 0x40; // Control byte (Co = 0, D/C = 1) for data
        memcpy(&data[1], &buffer[i], 16);
        helper_i2c_write(dev, data, sizeof(data));
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Funciones geometricas
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SSD1306_StartScrollRight(HelperI2CDevice *dev, uint8_t start, uint8_t stop)
{
    ssd1306_command(dev, SSD1306_RIGHT_HORIZONTAL_SCROLL);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, start);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, stop);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, 0XFF);
    ssd1306_command(dev, SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollLeft(HelperI2CDevice *dev, uint8_t start, uint8_t stop)
{
    ssd1306_command(dev, SSD1306_LEFT_HORIZONTAL_SCROLL);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, start);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, stop);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, 0XFF);
    ssd1306_command(dev, SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollDiagRight(HelperI2CDevice *dev, uint8_t start, uint8_t stop)
{
    ssd1306_command(dev, SSD1306_SET_VERTICAL_SCROLL_AREA);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, SSD1306_LCDHEIGHT);
    ssd1306_command(dev, SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, start);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, stop);
    ssd1306_command(dev, 0X01);
    ssd1306_command(dev, SSD1306_ACTIVATE_SCROLL);
}

void SSD1306_StartScrollDiagLeft(HelperI2CDevice *dev, uint8_t start, uint8_t stop)
{
    ssd1306_command(dev, SSD1306_SET_VERTICAL_SCROLL_AREA);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, SSD1306_LCDHEIGHT);
    ssd1306_command(dev, SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, start);
    ssd1306_command(dev, 0X00);
    ssd1306_command(dev, stop);
    ssd1306_command(dev, 0X01);
    ssd1306_command(dev, SSD1306_ACTIVATE_SCROLL);
}
//
void SSD1306_StopScroll(HelperI2CDevice *dev)
{
    ssd1306_command(dev, SSD1306_DEACTIVATE_SCROLL);
}
//
void SSD1306_Dim(HelperI2CDevice *dev, int dim)
{
    uint8_t contrast;
    if (dim)
        contrast = 0; // Dimmed display
    else
    {
        if (_vccstate == SSD1306_EXTERNALVCC)
            contrast = 0x9F;
        else
            contrast = 0xCF;
    }
    // the range of contrast to too small to be really useful
    // it is useful to dim the display
    ssd1306_command(dev, SSD1306_SETCONTRAST);
    ssd1306_command(dev, contrast);
}

// Limpiar el buffer del display
void SSD1306_ClearDisplay(void)
{
    memset(buffer, 0, sizeof(buffer));
}

void SSD1306_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int color)
{
    int steep;
    int8_t ystep;
    uint8_t dx, dy;
    int16_t err;
    steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        ssd1306_swap(x0, y0);
        ssd1306_swap(x1, y1);
    }
    if (x0 > x1)
    {
        ssd1306_swap(x0, x1);
        ssd1306_swap(y0, y1);
    }
    dx = x1 - x0;
    dy = abs(y1 - y0);

    err = dx / 2;
    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;

    for (; x0 <= x1; x0++)
    {
        if (steep)
        {
            if (color)
                SSD1306_DrawPixel(y0, x0, 1);
            else
                SSD1306_DrawPixel(y0, x0, 0);
        }
        else
        {
            if (color)
                SSD1306_DrawPixel(x0, y0, 1);
            else
                SSD1306_DrawPixel(x0, y0, 0);
        }
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}

void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t w, int color)
{
    SSD1306_DrawLine(x, y, x + w - 1, y, color);
}

void SSD1306_DrawFastVLine(uint8_t x, uint8_t y, uint8_t h, int color)
{
    SSD1306_DrawLine(x, y, x, y + h - 1, color);
}

void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int color)
{
    for (int16_t i = x; i < x + w; i++)
        SSD1306_DrawFastVLine(i, y, h, color);
}

void SSD1306_FillScreen(int color)
{
    SSD1306_FillRect(0, 0, SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, color);
}

void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    SSD1306_DrawPixel(x0, y0 + r, 1);
    SSD1306_DrawPixel(x0, y0 - r, 1);
    SSD1306_DrawPixel(x0 + r, y0, 1);
    SSD1306_DrawPixel(x0 - r, y0, 1);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        SSD1306_DrawPixel(x0 + x, y0 + y, 1);
        SSD1306_DrawPixel(x0 - x, y0 + y, 1);
        SSD1306_DrawPixel(x0 + x, y0 - y, 1);
        SSD1306_DrawPixel(x0 - x, y0 - y, 1);
        SSD1306_DrawPixel(x0 + y, y0 + x, 1);
        SSD1306_DrawPixel(x0 - y, y0 + x, 1);
        SSD1306_DrawPixel(x0 + y, y0 - x, 1);
        SSD1306_DrawPixel(x0 - y, y0 - x, 1);
    }
}

void SSD1306_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x4)
        {
            SSD1306_DrawPixel(x0 + x, y0 + y, 1);
            SSD1306_DrawPixel(x0 + y, y0 + x, 1);
        }
        if (cornername & 0x2)
        {
            SSD1306_DrawPixel(x0 + x, y0 - y, 1);
            SSD1306_DrawPixel(x0 + y, y0 - x, 1);
        }
        if (cornername & 0x8)
        {
            SSD1306_DrawPixel(x0 - y, y0 + x, 1);
            SSD1306_DrawPixel(x0 - x, y0 + y, 1);
        }
        if (cornername & 0x1)
        {
            SSD1306_DrawPixel(x0 - y, y0 - x, 1);
            SSD1306_DrawPixel(x0 - x, y0 - y, 1);
        }
    }
}

// Used to do circles and roundrects
void SSD1306_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, int color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x01)
        {
            SSD1306_DrawFastVLine(x0 + x, y0 - y, 2 * y + 1 + delta, color);
            SSD1306_DrawFastVLine(x0 + y, y0 - x, 2 * x + 1 + delta, color);
        }
        if (cornername & 0x02)
        {
            SSD1306_DrawFastVLine(x0 - x, y0 - y, 2 * y + 1 + delta, color);
            SSD1306_DrawFastVLine(x0 - y, y0 - x, 2 * x + 1 + delta, color);
        }
    }
}

// Draw a filled circle
void SSD1306_FillCircle(int16_t x0, int16_t y0, int16_t r, int color)
{
    SSD1306_DrawFastVLine(x0, y0 - r, 2 * r + 1, color);
    SSD1306_FillCircleHelper(x0, y0, r, 3, 0, color);
}

// Draw a rectangle
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    SSD1306_DrawFastHLine(x, y, w, 1);
    SSD1306_DrawFastHLine(x, y + h - 1, w, 1);
    SSD1306_DrawFastVLine(x, y, h, 1);
    SSD1306_DrawFastVLine(x + w - 1, y, h, 1);
}

// Draw a rounded rectangle
void SSD1306_DrawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r)
{
    // smarter version
    SSD1306_DrawFastHLine(x + r, y, w - 2 * r, 1);         // Top
    SSD1306_DrawFastHLine(x + r, y + h - 1, w - 2 * r, 1); // Bottom
    SSD1306_DrawFastVLine(x, y + r, h - 2 * r, 1);         // Left
    SSD1306_DrawFastVLine(x + w - 1, y + r, h - 2 * r, 1); // Right
    // draw four corners
    SSD1306_DrawCircleHelper(x + r, y + r, r, 1);
    SSD1306_DrawCircleHelper(x + w - r - 1, y + r, r, 2);
    SSD1306_DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4);
    SSD1306_DrawCircleHelper(x + r, y + h - r - 1, r, 8);
}

// Fill a rounded rectangle
void SSD1306_FillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, int color)
{
    // smarter version
    SSD1306_FillRect(x + r, y, w - 2 * r, h, color);
    // draw four corners
    SSD1306_FillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    SSD1306_FillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
}

// Draw a triangle
void SSD1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    SSD1306_DrawLine(x0, y0, x1, y1, 1);
    SSD1306_DrawLine(x1, y1, x2, y2, 1);
    SSD1306_DrawLine(x2, y2, x0, y0, 1);
}

// Fill a triangle
void SSD1306_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int color)
{
    int16_t a, b, y, last;
    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1)
    {
        ssd1306_swap(y0, y1);
        ssd1306_swap(x0, x1);
    }
    if (y1 > y2)
    {
        ssd1306_swap(y2, y1);
        ssd1306_swap(x2, x1);
    }
    if (y0 > y1)
    {
        ssd1306_swap(y0, y1);
        ssd1306_swap(x0, x1);
    }

    if (y0 == y2)
    { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if (x1 < a)
            a = x1;
        else if (x1 > b)
            b = x1;
        if (x2 < a)
            a = x2;
        else if (x2 > b)
            b = x2;
        SSD1306_DrawFastHLine(a, y0, b - a + 1, color);
        return;
    }

    int16_t
        dx01 = x1 - x0,
        dy01 = y1 - y0,
        dx02 = x2 - x0,
        dy02 = y2 - y0,
        dx12 = x2 - x1,
        dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    // is included here (and second loop will be skipped, avoiding a /0
    // error there), otherwise scanline y1 is skipped here and handled
    // in the second loop...which also avoids a /0 error here if y0=y1
    // (flat-topped triangle).
    if (y1 == y2)
        last = y1; // Include y1 scanline
    else
        last = y1 - 1; // Skip it

    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b)
            ssd1306_swap(a, b);
        SSD1306_DrawFastHLine(a, y, b - a + 1, color);
    }

    // For lower part of triangle, find scanline crossings for segments
    // 0-2 and 1-2.  This loop is skipped if y1=y2.
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b)
            ssd1306_swap(a, b);
        SSD1306_DrawFastHLine(a, y, b - a + 1, color);
    }
}
// Draw a pixel
void SSD1306_DrawPixel(uint8_t x, uint8_t y, int color)
{
    if ((x >= SSD1306_LCDWIDTH) || (y >= SSD1306_LCDHEIGHT))
        return;

    if (color)
        buffer[x + (uint16_t)(y / 8) * SSD1306_LCDWIDTH] |= (1 << (y & 7));
    else
        buffer[x + (uint16_t)(y / 8) * SSD1306_LCDWIDTH] &= ~(1 << (y & 7));
}
// Draw a character
void SSD1306_DrawChar(uint8_t x, uint8_t y, uint8_t c, uint8_t size)
{
    int8_t i, j;
    if ((x >= SSD1306_LCDWIDTH) || (y >= SSD1306_LCDHEIGHT))
        return;
    if (size < 1)
        size = 1;
    if ((c < ' ') || (c > '~'))
        c = '?';

    for (i = 0; i < 5; i++)
    {
        uint8_t line;
        if (c < 'S')
            line = font[(c - 32) * 5 + i];
        else
            line = font2[(c - 'S') * 5 + i];
        for (j = 0; j < 7; j++, line >>= 1)
        {
            if (line & 0x01)
            {
                if (size == 1)
                    SSD1306_DrawPixel(x + i, y + j, 1);
                else
                    SSD1306_FillRect(x + (i * size), y + (j * size), size, size, 1);
            }
            else
            {
                if (size == 1)
                    SSD1306_DrawPixel(x + i, y + j, 0); // delete pixel
                else
                    SSD1306_FillRect(x + i * size, y + j * size, size, size, 0);
            }
        }
    }
}
// Draw a string
void SSD1306_Drawtext(uint8_t x, uint8_t y, char *_text, uint8_t size)
{
    uint8_t cursor_x, cursor_y;
    uint16_t textsize, i;
    cursor_x = x, cursor_y = y;
    textsize = strlen(_text);

    for (i = 0; i < textsize; i++)
    {
        if (wrap && ((cursor_x + size * 5) > SSD1306_LCDWIDTH))
        {
            cursor_x = 0;
            cursor_y = cursor_y + size * 7 + 3;
            if (cursor_y > SSD1306_LCDHEIGHT)
                cursor_y = SSD1306_LCDHEIGHT;
            if (_text[i] == 0x20)
                goto _skip;
        }

        SSD1306_DrawChar(cursor_x, cursor_y, _text[i], size);
        cursor_x = cursor_x + size * 6;
        if (cursor_x > SSD1306_LCDWIDTH)
            cursor_x = SSD1306_LCDWIDTH;
    _skip:;
    }
}
// Set text wrap
void SSD1306_SetTextWrap(int w)
{
    wrap = w;
}
//  Invert the display
void SSD1306_InvertDisplay(HelperI2CDevice *dev, int i)
{
    if (i)
        ssd1306_command(dev, SSD1306_INVERTDISPLAY_);
    else
        ssd1306_command(dev, SSD1306_NORMALDISPLAY);
}

// Bitmap vertical de LCD Assistant (page-oriented)
void SSD1306_DrawBMP(HelperI2CDevice *dev, uint8_t x, uint8_t y, const uint8_t *bitmap, uint16_t w, uint16_t h)
{

    for (uint16_t i = 0; i < h / 8; i++)
    {
        for (uint16_t j = 0; j < (uint16_t)w * 8; j++)
        {
            if (bit_test(bitmap[j / 8 + i * w], j % 8) == 1)
                SSD1306_DrawPixel(x + j / 8, y + i * 8 + (j % 8), 1);
            else
                SSD1306_DrawPixel(x + j / 8, y + i * 8 + (j % 8), 0);
        }
    }
}
