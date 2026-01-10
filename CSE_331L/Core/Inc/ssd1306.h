#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT   64

// Most common address is 0x3C. If blank screen, change to 0x3D.
#define SSD1306_I2C_ADDR (0x3C << 1)

typedef enum {
    Black = 0x00,
    White = 0x01
} SSD1306_COLOR;

void SSD1306_Init(I2C_HandleTypeDef *hi2c);
void SSD1306_UpdateScreen(void);
void SSD1306_Fill(SSD1306_COLOR color);
void SSD1306_SetCursor(uint8_t x, uint8_t y);
void SSD1306_WriteChar(char ch);
void SSD1306_WriteString(const char *str);

#endif
