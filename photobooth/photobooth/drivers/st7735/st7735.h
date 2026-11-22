/**
 * @file    st7735.h
 * @brief   Driver for the Adafruit 1.8" 160x128 TFT (ST7735R controller,
 *          "black tab" version), write-only over SPI.
 *
 * How the display is driven
 * -------------------------
 *  - Every byte is either a COMMAND (D/C low) or DATA (D/C high).
 *  - The controller has its own frame memory (GRAM), so the MCU only sends
 *    what changes: set a window (CASET/RASET), send RAMWR, then stream
 *    w x h pixels. The window auto-increments, no addresses per pixel.
 *  - Pixel format RGB565, 2 bytes per pixel, high byte first.
 *
 * Memory: a full screen is 160 x 128 x 2 = 40 960 B, more than all of SRAM.
 * So nothing frame-sized lives here: callers stream pixels row by row
 * (st7735_window() then st7735_write_pixels()), e.g. from a 320-byte row.
 */
#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>
#include <stdbool.h>

/** Screen size in the orientation set by ST7735_MADCTL (landscape). */
#define ST7735_WIDTH    (160U)
#define ST7735_HEIGHT   (128U)

/** Return code of every driver function. */
typedef enum
{
    ST7735_OK = 0,          /**< success                                     */
    ST7735_ERR_PARAM,       /**< window outside the screen, NULL pointer     */
    ST7735_ERR_NOT_INIT     /**< st7735_init() has not been called           */
} st7735_status_t;

/** Pack 8-bit R, G, B into RGB565. */
#define ST7735_RGB(r, g, b) \
    ((uint16_t)((((uint16_t)(r) & 0xF8U) << 8U) | \
                (((uint16_t)(g) & 0xFCU) << 3U) | \
                ((uint16_t)(b) >> 3U)))

#define ST7735_BLACK    (0x0000U)
#define ST7735_WHITE    (0xFFFFU)
#define ST7735_RED      (0xF800U)
#define ST7735_GREEN    (0x07E0U)
#define ST7735_BLUE     (0x001FU)

/**
 * Set up SPI and the CS/DC/RESET pins, hardware-reset the panel and send
 * the ST7735R initialisation sequence. Leaves the screen black. ~0.8 s
 * (the controller needs long delays after reset and sleep-out).
 * The display cannot be read back, so this cannot detect a missing panel.
 */
st7735_status_t st7735_init(void);

/**
 * Select the rectangle [x, x+w) x [y, y+h) and start a pixel write.
 * Follow with exactly w*h pixels via st7735_write_pixels() (any number of
 * calls), then st7735_end().
 */
st7735_status_t st7735_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/** Send @p count RGB565 pixels into the current window. */
st7735_status_t st7735_write_pixels(const uint16_t *pixels, uint32_t count);

/** Send the same colour @p count times into the current window. */
st7735_status_t st7735_write_color(uint16_t color, uint32_t count);

/** Finish a pixel write (waits for SPI to finish, releases CS). */
void st7735_end(void);

/** Fill a rectangle with one colour. */
st7735_status_t st7735_fill_rect(uint16_t x, uint16_t y, uint16_t w,
                                 uint16_t h, uint16_t color);

#endif /* ST7735_H */
