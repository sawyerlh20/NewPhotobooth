/**
 * @file    st7735.c
 * @brief   ST7735R TFT driver (see st7735.h).
 *
 * Init sequence: Adafruit-ST7735-Library "Rcmd1 + Rcmd2red + Rcmd3"
 * (initR(INITR_BLACKTAB)), which is the sequence for this breakout.
 * Command reference: Sitronix ST7735R datasheet, section 10 (command list).
 */
#include "drivers/st7735/st7735.h"

#include <stddef.h>

#include "bsp/board.h"
#include "bsp/bsp.h"

/* TODO: include the TivaWare headers you need. */

/* ---- Configuration ------------------------------------------------------- */

/**
 * Memory access control (MADCTL, 0x36): sets the orientation.
 * bit 7 MY (row order), bit 6 MX (column order), bit 5 MV (swap rows and
 * columns = landscape), bit 3 BGR (colour order).
 *   0xA0 = MY|MV  landscape, connector on the left  (Adafruit rotation 1)
 *   0x60 = MX|MV  landscape, connector on the right (Adafruit rotation 3)
 * If red and blue are swapped on the 'L' test, add 0x08.
 */
#define ST7735_MADCTL       (0xA0U)

/** GRAM offset of the visible area. 0/0 for the black-tab panel; some
 *  green-tab panels need 1/2 (look for a garbage line at one edge). */
#define ST7735_X_OFFSET     (0U)
#define ST7735_Y_OFFSET     (0U)

/* Command codes (ST7735R datasheet, section 10.1). */
#define CMD_SWRESET   (0x01U)
#define CMD_SLPOUT    (0x11U)
#define CMD_NORON     (0x13U)
#define CMD_INVOFF    (0x20U)
#define CMD_DISPON    (0x29U)
#define CMD_CASET     (0x2AU)
#define CMD_RASET     (0x2BU)
#define CMD_RAMWR     (0x2CU)
#define CMD_MADCTL    (0x36U)
#define CMD_COLMOD    (0x3AU)
#define CMD_FRMCTR1   (0xB1U)
#define CMD_FRMCTR2   (0xB2U)
#define CMD_FRMCTR3   (0xB3U)
#define CMD_INVCTR    (0xB4U)
#define CMD_PWCTR1    (0xC0U)
#define CMD_PWCTR2    (0xC1U)
#define CMD_PWCTR3    (0xC2U)
#define CMD_PWCTR4    (0xC3U)
#define CMD_PWCTR5    (0xC4U)
#define CMD_VMCTR1    (0xC5U)
#define CMD_GMCTRP1   (0xE0U)
#define CMD_GMCTRN1   (0xE1U)

#define INIT_MAX_ARGS (16U)

/** One step of the init sequence: command, its data bytes, then a delay. */
typedef struct
{
    uint8_t  cmd;
    uint8_t  count;                  /* number of valid bytes in args */
    uint8_t  args[INIT_MAX_ARGS];
    uint16_t delay_ms;               /* wait after the command        */
} init_step_t;

static const init_step_t s_init[] =
{
    { CMD_SWRESET, 0U, { 0U }, 150U },            /* software reset          */
    { CMD_SLPOUT,  0U, { 0U }, 500U },            /* leave sleep mode        */
    { CMD_FRMCTR1, 3U, { 0x01U, 0x2CU, 0x2DU }, 0U },           /* frame rate */
    { CMD_FRMCTR2, 3U, { 0x01U, 0x2CU, 0x2DU }, 0U },
    { CMD_FRMCTR3, 6U, { 0x01U, 0x2CU, 0x2DU, 0x01U, 0x2CU, 0x2DU }, 0U },
    { CMD_INVCTR,  1U, { 0x07U }, 0U },           /* no line inversion       */
    { CMD_PWCTR1,  3U, { 0xA2U, 0x02U, 0x84U }, 0U },           /* power      */
    { CMD_PWCTR2,  1U, { 0xC5U }, 0U },
    { CMD_PWCTR3,  2U, { 0x0AU, 0x00U }, 0U },
    { CMD_PWCTR4,  2U, { 0x8AU, 0x2AU }, 0U },
    { CMD_PWCTR5,  2U, { 0x8AU, 0xEEU }, 0U },
    { CMD_VMCTR1,  1U, { 0x0EU }, 0U },           /* VCOM                    */
    { CMD_INVOFF,  0U, { 0U }, 0U },
    { CMD_MADCTL,  1U, { ST7735_MADCTL }, 0U },   /* orientation             */
    { CMD_COLMOD,  1U, { 0x05U }, 0U },           /* 16-bit RGB565           */
    { CMD_GMCTRP1, 16U, { 0x02U, 0x1CU, 0x07U, 0x12U, 0x37U, 0x32U, 0x29U,
                          0x2DU, 0x29U, 0x25U, 0x2BU, 0x39U, 0x00U, 0x01U,
                          0x03U, 0x10U }, 0U },   /* gamma (+)               */
    { CMD_GMCTRN1, 16U, { 0x03U, 0x1DU, 0x07U, 0x06U, 0x2EU, 0x2CU, 0x29U,
                          0x2DU, 0x2EU, 0x2EU, 0x37U, 0x3FU, 0x00U, 0x00U,
                          0x02U, 0x10U }, 0U },   /* gamma (-)               */
    { CMD_NORON,   0U, { 0U }, 10U },             /* normal display mode     */
    { CMD_DISPON,  0U, { 0U }, 100U }             /* display on              */
};

#define INIT_STEPS  ((uint32_t)(sizeof(s_init) / sizeof(s_init[0])))

static bool s_ready = false;

/* ---- Low level ----------------------------------------------------------- */

/** Select the panel (chip select active). */
static void cs_low(void)
{
    /* TODO */
}

/** Deselect the panel. */
static void cs_high(void)
{
    /* TODO */
}

/** Wait until the last byte has completely left the SPI peripheral. */
static void spi_wait_idle(void)
{
    /* TODO */
}

/** D/C low = command, high = data. */
static void dc_set(bool data)
{
    /* TODO: drive the D/C pin. Think about WHEN it is safe to change it. */
    (void)data;
}

/** Send one byte over SPI. */
static void spi_put(uint8_t value)
{
    /* TODO */
    (void)value;
}

static void send_command(uint8_t cmd, const uint8_t *args, uint32_t count)
{
    uint32_t i;

    dc_set(false);
    spi_put(cmd);
    if (count > 0U)
    {
        dc_set(true);
        for (i = 0U; i < count; i++)
        {
            spi_put(args[i]);
        }
    }
}

/* ---- Public API ---------------------------------------------------------- */

st7735_status_t st7735_init(void)
{
    uint32_t i;

    /* TODO: set up the SPI peripheral as a master (pick the SPI mode and
     * clock speed from the ST7735R datasheet), its pins, and the CS, D/C
     * and RESET pins (see board.h). CS should start deselected. */

    /* TODO: hardware-reset the panel with the RESET pin (see the reset
     * timing in the ST7735R datasheet). */

    cs_low();
    for (i = 0U; i < INIT_STEPS; i++)
    {
        send_command(s_init[i].cmd, s_init[i].args, (uint32_t)s_init[i].count);
        if (s_init[i].delay_ms > 0U)
        {
            spi_wait_idle();
            bsp_delay_ms((uint32_t)s_init[i].delay_ms);
        }
    }
    spi_wait_idle();
    cs_high();
    s_ready = true;

    return st7735_fill_rect(0U, 0U, ST7735_WIDTH, ST7735_HEIGHT, ST7735_BLACK);
}

st7735_status_t st7735_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    st7735_status_t status = ST7735_OK;
    uint8_t args[4];
    uint16_t x0;
    uint16_t x1;
    uint16_t y0;
    uint16_t y1;

    if (!s_ready)
    {
        status = ST7735_ERR_NOT_INIT;
    }
    else if ((w == 0U) || (h == 0U) ||
             (((uint32_t)x + (uint32_t)w) > ST7735_WIDTH) ||
             (((uint32_t)y + (uint32_t)h) > ST7735_HEIGHT))
    {
        status = ST7735_ERR_PARAM;
    }
    else
    {
        /* Inclusive start/end addresses, 16-bit big-endian. */
        x0 = (uint16_t)(x + ST7735_X_OFFSET);
        x1 = (uint16_t)(x0 + w - 1U);
        y0 = (uint16_t)(y + ST7735_Y_OFFSET);
        y1 = (uint16_t)(y0 + h - 1U);

        cs_low();
        args[0] = (uint8_t)(x0 >> 8U);
        args[1] = (uint8_t)(x0 & 0xFFU);
        args[2] = (uint8_t)(x1 >> 8U);
        args[3] = (uint8_t)(x1 & 0xFFU);
        send_command(CMD_CASET, args, 4U);
        args[0] = (uint8_t)(y0 >> 8U);
        args[1] = (uint8_t)(y0 & 0xFFU);
        args[2] = (uint8_t)(y1 >> 8U);
        args[3] = (uint8_t)(y1 & 0xFFU);
        send_command(CMD_RASET, args, 4U);
        send_command(CMD_RAMWR, NULL, 0U);
        dc_set(true);                         /* pixel data follows */
    }
    return status;
}

st7735_status_t st7735_write_pixels(const uint16_t *pixels, uint32_t count)
{
    st7735_status_t status = ST7735_OK;
    uint32_t i;

    if (!s_ready)
    {
        status = ST7735_ERR_NOT_INIT;
    }
    else if ((pixels == NULL) && (count > 0U))
    {
        status = ST7735_ERR_PARAM;
    }
    else
    {
        for (i = 0U; i < count; i++)
        {
            spi_put((uint8_t)(pixels[i] >> 8U));
            spi_put((uint8_t)(pixels[i] & 0xFFU));
        }
    }
    return status;
}

st7735_status_t st7735_write_color(uint16_t color, uint32_t count)
{
    st7735_status_t status = ST7735_OK;
    uint32_t i;

    if (!s_ready)
    {
        status = ST7735_ERR_NOT_INIT;
    }
    else
    {
        for (i = 0U; i < count; i++)
        {
            spi_put((uint8_t)(color >> 8U));
            spi_put((uint8_t)(color & 0xFFU));
        }
    }
    return status;
}

void st7735_end(void)
{
    spi_wait_idle();
    cs_high();
}

st7735_status_t st7735_fill_rect(uint16_t x, uint16_t y, uint16_t w,
                                 uint16_t h, uint16_t color)
{
    st7735_status_t status = st7735_window(x, y, w, h);

    if (status == ST7735_OK)
    {
        status = st7735_write_color(color, (uint32_t)w * (uint32_t)h);
        st7735_end();
    }
    return status;
}
