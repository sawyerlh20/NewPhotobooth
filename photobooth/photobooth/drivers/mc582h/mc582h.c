/**
 * @file    mc582h.c
 * @brief   MC582H thermal printer driver (see mc582h.h).
 */
#include "drivers/mc582h/mc582h.h"

#include <stddef.h>

#include "bsp/board.h"
#include "bsp/bsp.h"

/* TODO: include the TivaWare headers you need. */

/* ESC/POS control characters. */
#define PRN_ESC   (0x1BU)
#define PRN_GS    (0x1DU)
#define PRN_DLE   (0x10U)
#define PRN_EOT   (0x04U)
#define PRN_FS    (0x1CU)

/** How long to wait for a status reply (the printer answers in ~ms). */
#define PRN_REPLY_TIMEOUT_MS   (200U)

/** GS v 0 limits: width bytes are a 16-bit field, height max 2047 here. */
#define PRN_RASTER_MAX_HEIGHT  (2047U)

static bool s_ready = false;

/** Queue one byte (waits while the 16-byte TX FIFO is full). */
static void put_byte(uint8_t value)
{
    /* TODO: send one byte to the printer over the UART. */
    (void)value;
}

mc582h_status_t mc582h_init(void)
{
    /* ESC @ : reset settings.
     * FS .  : cancel Chinese character mode (on by default on this unit),
     *         otherwise bytes >= 0x80 are printed as double-byte glyphs. */
    static const uint8_t reset_cmd[4] = { PRN_ESC, (uint8_t)'@',
                                          PRN_FS,  (uint8_t)'.' };

    /* TODO: set up the printer UART and its two pins (see board.h), with
     * the serial settings the printer expects. */
    s_ready = true;

    return mc582h_write(reset_cmd, (uint32_t)sizeof(reset_cmd));
}

mc582h_status_t mc582h_write(const uint8_t *data, uint32_t length)
{
    mc582h_status_t status = MC582H_OK;
    uint32_t i;

    if (!s_ready)
    {
        status = MC582H_ERR_NOT_INIT;
    }
    else if ((data == NULL) && (length > 0U))
    {
        status = MC582H_ERR_PARAM;
    }
    else
    {
        for (i = 0U; i < length; i++)
        {
            put_byte(data[i]);
        }
    }
    return status;
}

mc582h_status_t mc582h_print_text(const char *text)
{
    mc582h_status_t status = MC582H_OK;
    uint32_t length = 0U;

    if (text == NULL)
    {
        status = MC582H_ERR_PARAM;
    }
    else
    {
        while (text[length] != '\0')
        {
            length++;
        }
        status = mc582h_write((const uint8_t *)text, length);
    }
    return status;
}

mc582h_status_t mc582h_feed(uint8_t lines)
{
    const uint8_t cmd[3] = { PRN_ESC, (uint8_t)'d', lines };
    return mc582h_write(cmd, (uint32_t)sizeof(cmd));
}

mc582h_status_t mc582h_cut(uint8_t feed)
{
    /* GS V 66 n: feed to the cutter position plus n dot rows, then
     * partial cut (66 is the form most clone cutters support). */
    const uint8_t cmd[4] = { PRN_GS, (uint8_t)'V', 66U, feed };
    return mc582h_write(cmd, (uint32_t)sizeof(cmd));
}

mc582h_status_t mc582h_raster_begin(uint16_t width_bytes, uint16_t height)
{
    mc582h_status_t status;
    uint8_t header[8];

    if ((width_bytes == 0U) || (height == 0U) ||
        (height > PRN_RASTER_MAX_HEIGHT))
    {
        status = MC582H_ERR_PARAM;
    }
    else
    {
        /* GS v 0 m xL xH yL yH: m = 0 normal size, x in bytes, y in dots. */
        header[0] = PRN_GS;
        header[1] = (uint8_t)'v';
        header[2] = (uint8_t)'0';
        header[3] = 0U;
        header[4] = (uint8_t)(width_bytes & 0xFFU);
        header[5] = (uint8_t)(width_bytes >> 8U);
        header[6] = (uint8_t)(height & 0xFFU);
        header[7] = (uint8_t)(height >> 8U);
        status = mc582h_write(header, (uint32_t)sizeof(header));
    }
    return status;
}

mc582h_status_t mc582h_print_raster(const uint8_t *bits, uint16_t width_bytes,
                                    uint16_t height)
{
    mc582h_status_t status;

    if (bits == NULL)
    {
        status = MC582H_ERR_PARAM;
    }
    else
    {
        status = mc582h_raster_begin(width_bytes, height);
        if (status == MC582H_OK)
        {
            status = mc582h_write(bits, (uint32_t)width_bytes * (uint32_t)height);
        }
    }
    return status;
}

mc582h_status_t mc582h_query_status(mc582h_status_class_t which,
                                    uint8_t *status)
{
    mc582h_status_t result = MC582H_ERR_TIMEOUT;
    uint8_t cmd[3];
    uint32_t ms;
    int32_t rx;

    if (status == NULL)
    {
        result = MC582H_ERR_PARAM;
    }
    else if (!s_ready)
    {
        result = MC582H_ERR_NOT_INIT;
    }
    else
    {
        /* TODO: throw away anything already received (e.g. noise at
         * power-up) so the reply below is really the answer. */

        cmd[0] = PRN_DLE;
        cmd[1] = PRN_EOT;
        cmd[2] = (uint8_t)which;
        (void)mc582h_write(cmd, (uint32_t)sizeof(cmd));

        for (ms = 0U; (ms < PRN_REPLY_TIMEOUT_MS) &&
                      (result == MC582H_ERR_TIMEOUT); ms++)
        {
            /* TODO: read one received byte WITHOUT blocking; set rx to the
             * byte, or to -1 if nothing has arrived yet. */
            rx = -1;
            if (rx >= 0)
            {
                *status = (uint8_t)rx;
                result  = MC582H_OK;
            }
            else
            {
                bsp_delay_ms(1U);
            }
        }
    }
    return result;
}
