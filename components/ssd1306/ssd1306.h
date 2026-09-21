/******************************************************************************
 SSD1306 OLED driver adapted for ESP32 IDF (ssd1306.h)                        *
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

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "helper_i2c.h" //

/******************************************************************************
 I2C
 ******************************************************************************/
#define I2C_MASTER_FREQ_HZ 400000 /*!< Frecuencia I2C */
#define SSD1306_I2C_ADDRESS 0x3C  /*!< Dirección I2C del SSD1306 */
//#define SSD1306_I2C_ADDRESS 0x3D  /*!< Dirección I2C del SSD1306 */

/*****************************************************************************/

#define ssd1306_swap(a, b) \
    {                      \
        int16_t t = a;     \
        a = b;             \
        b = t;             \
    }

#define BIT_TEST(byte, bit) (((byte) >> (bit)) & 1)

#if !defined SSD1306_128_32 && !defined SSD1306_96_16
#define SSD1306_128_64
#endif
#if defined SSD1306_128_32 && defined SSD1306_96_16
#error "Only one SSD1306 display can be specified at once"
#endif

#if defined SSD1306_128_64
#define SSD1306_LCDWIDTH 128
#define SSD1306_LCDHEIGHT 64
#endif
#if defined SSD1306_128_32
#define SSD1306_LCDWIDTH 128
#define SSD1306_LCDHEIGHT 32
#endif
#if defined SSD1306_96_16
#define SSD1306_LCDWIDTH 96
#define SSD1306_LCDHEIGHT 16
#endif

#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY_ 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x01
#define SSD1306_SWITCHCAPVCC 0x02
/******************************************************************************
 SSD1306 ID and Command List
 ******************************************************************************/
#define SSD1306_ADDRESS 0x3C
// #define SSD1306_COMMAND      0x00
#define SSD1306_DATA 0xC0
#define SSD1306_DATA_CONTINUE 0x40
#define SSD1306_SET_CONTRAST_CONTROL 0x81
#define SSD1306_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD1306_DISPLAY_ALL_ON 0xA5
#define SSD1306_NORMAL_DISPLAY 0xA6
#define SSD1306_INVERT_DISPLAY 0xA7
#define SSD1306_DISPLAY_OFF 0xAE
#define SSD1306_DISPLAY_ON 0xAF
#define SSD1306_NOP 0xE3
#define SSD1306_HORIZONTAL_SCROLL_RIGHT 0x26
#define SSD1306_HORIZONTAL_SCROLL_LEFT 0x27
#define SSD1306_HORIZONTAL_SCROLL_VERTICAL_AND_RIGHT 0x29
#define SSD1306_HORIZONTAL_SCROLL_VERTICAL_AND_LEFT 0x2A
#define SSD1306_SET_LOWER_COLUMN 0x00
#define SSD1306_SET_HIGHER_COLUMN 0x10
#define SSD1306_MEMORY_ADDR_MODE 0x20
#define SSD1306_SET_COLUMN_ADDR 0x21
#define SSD1306_SET_PAGE_ADDR 0x22
#define SSD1306_SET_START_LINE 0x40
#define SSD1306_SET_SEGMENT_REMAP 0xA0
#define SSD1306_SET_MULTIPLEX_RATIO 0xA8
#define SSD1306_COM_SCAN_DIR_INC 0xC0
#define SSD1306_COM_SCAN_DIR_DEC 0xC8
#define SSD1306_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_SET_COM_PINS 0xDA
#define SSD1306_CHARGE_PUMP 0x8D
#define SSD1306_SET_DISPLAY_CLOCK_DIV_RATIO 0xD5
#define SSD1306_SET_PRECHARGE_PERIOD 0xD9
#define SSD1306_SET_VCOM_DESELECT 0xDB
/****************************************************************************/
// Scrolling #defines
#define SSD1306_ACTIVATE_SCROLL 0x2F //
#define SSD1306_DEACTIVATE_SCROLL 0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA 0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL 0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL 0x2A

// Prototipos de funciones
void ssd1306_command(HelperI2CDevice *dev, uint8_t c);                            // Enviar comando
esp_err_t SSD1306_Begin(HelperI2CDevice *dev, uint8_t vccstate, uint8_t i2caddr); // Inicializar Display

void SSD1306_Drawtext(uint8_t x, uint8_t y, char *_text, uint8_t size);
void SSD1306_SetTextWrap(int w);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, int color);

void SSD1306_StartScrollRight(HelperI2CDevice *dev, uint8_t start, uint8_t stop);
void SSD1306_StartScrollLeft(HelperI2CDevice *dev, uint8_t start, uint8_t stop);
void SSD1306_StartScrollDiagRight(HelperI2CDevice *dev, uint8_t start, uint8_t stop);
void SSD1306_StartScrollDiagLeft(HelperI2CDevice *dev, uint8_t start, uint8_t stop);
void SSD1306_StopScroll(HelperI2CDevice *dev);
void SSD1306_Dim(HelperI2CDevice *dev, int dim);
void SSD1306_Display(HelperI2CDevice *dev);
void SSD1306_ClearDisplay(void);
void SSD1306_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int color);
void SSD1306_DrawFastHLine(uint8_t x, uint8_t y, uint8_t w, int color);
void SSD1306_DrawFastVLine(uint8_t x, uint8_t y, uint8_t h, int color);
void SSD1306_FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int color);
void SSD1306_FillScreen(int color);
void SSD1306_DrawCircle(int16_t x0, int16_t y0, int16_t r);
void SSD1306_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername);
void SSD1306_FillCircle(int16_t x0, int16_t y0, int16_t r, int color);
void SSD1306_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, int color);
void SSD1306_DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void SSD1306_DrawRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
void SSD1306_FillRoundRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r, int color);
void SSD1306_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void SSD1306_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int color);
void SSD1306_DrawChar(uint8_t x, uint8_t y, uint8_t c, uint8_t size);
void SSD1306_Drawtext(uint8_t x, uint8_t y, char *_text, uint8_t size);

void SSD1306_SetTextWrap(int w);
void SSD1306_InvertDisplay(HelperI2CDevice *dev, int i);
void SSD1306_DrawBMP(HelperI2CDevice *dev, uint8_t x, uint8_t y, const uint8_t *bitmap, uint16_t w, uint16_t h);

#endif // SSD1306_H

// Add new Function
// void SSD1306_DrawBitmap(uint8_t x, uint8_t y,char *image);
// void SSD1306_Icon(char *icon, uint8_t seg, uint8_t pag, uint8_t _width, uint8_t _height);
// void SSD1306_Image(char *image);
// void SSD1306_Command(char cmd);
// void SSD1306_SetPointer(HelperI2CDevice *dev, uint8_t seg, uint8_t pag);
// void SSD1306_ROMBMP(uint8_t x, uint8_t y, uint8_t *bitmap, uint8_t w, uint8_t h);
// void SSD1306_Icon(HelperI2CDevice *dev, char *icon, uint8_t seg, uint8_t pag, uint8_t _width, uint8_t _height);
// void SSD1306_WriteRam(char dat);
// void SSD1306_drawPercentbar(int x,int y, int width,int height, int progress);
