#include "ssd1306.h"
#include "ssd1306_font.h"

static I2C_HandleTypeDef *ssd1306_i2c;

static uint8_t SSD1306_Buffer[SSD1306_WIDTH * (SSD1306_HEIGHT / 8)];
static uint8_t CurrentX = 0;
static uint8_t CurrentY = 0;

static void SSD1306_WriteCommand(uint8_t byte)
{
    uint8_t data[2];
    data[0] = 0x00;   // Control byte: 0x00 = command
    data[1] = byte;
    HAL_I2C_Master_Transmit(ssd1306_i2c, SSD1306_I2C_ADDR, data, 2, HAL_MAX_DELAY);
}

static void SSD1306_WriteData(uint8_t *data, uint16_t size)
{
    // Control byte 0x40 = data
    // Send in chunks because I2C buffer size can be limited
    uint8_t temp[17];
    temp[0] = 0x40;

    while (size > 0)
    {
        uint16_t chunk = (size > 16) ? 16 : size;
        memcpy(&temp[1], data, chunk);
        HAL_I2C_Master_Transmit(ssd1306_i2c, SSD1306_I2C_ADDR, temp, chunk + 1, HAL_MAX_DELAY);

        data += chunk;
        size -= chunk;
    }
}

void SSD1306_Init(I2C_HandleTypeDef *hi2c)
{
    ssd1306_i2c = hi2c;

    HAL_Delay(100);

    // Init sequence (SSD1306 128x64)
    SSD1306_WriteCommand(0xAE); // display off

    SSD1306_WriteCommand(0x20); // Set Memory Addressing Mode
    SSD1306_WriteCommand(0x00); // 00 = Horizontal Addressing Mode

    SSD1306_WriteCommand(0xB0); // Set Page Start Address for Page Addressing Mode

    SSD1306_WriteCommand(0xC8); // COM Output Scan Direction (remapped)

    SSD1306_WriteCommand(0x00); // low column address
    SSD1306_WriteCommand(0x10); // high column address

    SSD1306_WriteCommand(0x40); // start line address

    SSD1306_WriteCommand(0x81); // contrast control
    SSD1306_WriteCommand(0x7F);

    SSD1306_WriteCommand(0xA1); // segment re-map

    SSD1306_WriteCommand(0xA6); // normal display

    SSD1306_WriteCommand(0xA8); // multiplex ratio
    SSD1306_WriteCommand(0x3F); // 1/64

    SSD1306_WriteCommand(0xA4); // display follows RAM

    SSD1306_WriteCommand(0xD3); // display offset
    SSD1306_WriteCommand(0x00);

    SSD1306_WriteCommand(0xD5); // display clock divide ratio/osc freq
    SSD1306_WriteCommand(0x80);

    SSD1306_WriteCommand(0xD9); // pre-charge period
    SSD1306_WriteCommand(0xF1);

    SSD1306_WriteCommand(0xDA); // com pins hardware config
    SSD1306_WriteCommand(0x12);

    SSD1306_WriteCommand(0xDB); // vcomh deselect level
    SSD1306_WriteCommand(0x40);

    SSD1306_WriteCommand(0x8D); // charge pump
    SSD1306_WriteCommand(0x14);

    SSD1306_WriteCommand(0xAF); // display ON

    SSD1306_Fill(Black);
    SSD1306_UpdateScreen();
}

void SSD1306_UpdateScreen(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_WriteCommand(0xB0 + page); // page address
        SSD1306_WriteCommand(0x00);        // low column
        SSD1306_WriteCommand(0x10);        // high column

        SSD1306_WriteData(&SSD1306_Buffer[SSD1306_WIDTH * page], SSD1306_WIDTH);
    }
}

void SSD1306_Fill(SSD1306_COLOR color)
{
    memset(SSD1306_Buffer, (color == White) ? 0xFF : 0x00, sizeof(SSD1306_Buffer));
}

void SSD1306_SetCursor(uint8_t x, uint8_t y)
{
    CurrentX = x;
    CurrentY = y;
}

// Draw pixel into buffer
static void SSD1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    if (color == White)
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= (1 << (y % 8));
    else
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
}

void SSD1306_WriteChar(char ch)
{
    if (ch < 32 || ch > 127) ch = '?';
    uint8_t index = (uint8_t)(ch - 32);

    // Each char is 5 columns wide, plus 1 column space
    for (uint8_t col = 0; col < 5; col++)
    {
        uint8_t line = Font5x7[index][col];
        for (uint8_t row = 0; row < 8; row++)
        {
            SSD1306_DrawPixel(CurrentX + col, CurrentY + row, (line & (1 << row)) ? White : Black);
        }
    }

    // one column spacing
    for (uint8_t row = 0; row < 8; row++)
    {
        SSD1306_DrawPixel(CurrentX + 5, CurrentY + row, Black);
    }

    CurrentX += 6; // move cursor
}

void SSD1306_WriteString(const char *str)
{
    while (*str)
    {
        if (CurrentX + 6 >= SSD1306_WIDTH)
        {
            CurrentX = 0;
            CurrentY += 8; // next line
        }
        if (CurrentY + 8 >= SSD1306_HEIGHT) break;

        SSD1306_WriteChar(*str);
        str++;
    }
}
