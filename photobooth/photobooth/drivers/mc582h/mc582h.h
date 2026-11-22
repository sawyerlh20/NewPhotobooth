/**
 * @file    mc582h.h
 * @brief   Driver for the MC582H 58 mm thermal receipt printer (TTL UART,
 *          ESC/POS command set).
 *
 * What is known about this printer (from its self-test page)
 * ---------------------------------------------------------
 *  - Command mode EPSON (ESC/POS), serial 115200 8N1, 32 chars/line (font A).
 *  - Supply 12-24 V; the peak current is unknown. Thermal heads draw large
 *    current pulses, so do NOT power it from the LaunchPad.
 *
 * NOT verified yet (bring-up checks these, in this order):
 *  1. mc582h_query_status(): does the printer answer DLE EOT? (no paper)
 *  2. mc582h_print_text():   does it accept 3.3 V TX?        (one line)
 *  3. mc582h_print_raster(): is GS v 0 supported, and what bit order?
 *  Still unknown: printable width in dots, receive-buffer size, flow
 *  control. Until those are known, keep raster images small.
 *
 * ESC/POS reference: https://download4.epson.biz/sec_pubs/pos/reference_en/escpos/
 */
#ifndef MC582H_H
#define MC582H_H

#include <stdint.h>
#include <stdbool.h>

/** Return code of every driver function. */
typedef enum
{
    MC582H_OK = 0,          /**< success                                     */
    MC582H_ERR_PARAM,       /**< NULL pointer or out-of-range argument       */
    MC582H_ERR_NOT_INIT,    /**< mc582h_init() has not been called           */
    MC582H_ERR_TIMEOUT      /**< printer did not answer (RX wiring? command
                                 not supported?)                             */
} mc582h_status_t;

/** Status classes for mc582h_query_status() (DLE EOT n). */
typedef enum
{
    MC582H_STATUS_PRINTER = 1,  /**< online/offline, drawer pin              */
    MC582H_STATUS_OFFLINE = 2,  /**< cover open, FEED pressed, error         */
    MC582H_STATUS_ERROR   = 3,  /**< cutter / unrecoverable / auto-recover   */
    MC582H_STATUS_PAPER   = 4   /**< paper near-end / paper end              */
} mc582h_status_class_t;

/**
 * Set up the printer UART, send ESC @ (reset the printer's settings) and
 * FS . (Chinese character mode off). Prints nothing.
 */
mc582h_status_t mc582h_init(void);

/** Send raw bytes (any ESC/POS command). */
mc582h_status_t mc582h_write(const uint8_t *data, uint32_t length);

/** Send a text string as-is. Text is printed when '\n' is received. */
mc582h_status_t mc582h_print_text(const char *text);

/** Feed the paper by @p lines text lines (ESC d n). */
mc582h_status_t mc582h_feed(uint8_t lines);

/**
 * Feed to the cutter and cut (GS V 66 n, partial cut). Verified on this
 * unit: a cutter is fitted and this command cuts. The feed to the blade is
 * automatic, so the last printed row is not cut through.
 *
 * @param feed extra dot rows to feed before cutting (0 is fine)
 */
mc582h_status_t mc582h_cut(uint8_t feed);

/**
 * Start a GS v 0 image of @p height rows; then send exactly
 * width_bytes * height bytes with mc582h_write(), row by row (same format
 * as mc582h_print_raster()). Lets an image be streamed from a small row
 * buffer instead of being held in RAM.
 */
mc582h_status_t mc582h_raster_begin(uint16_t width_bytes, uint16_t height);

/**
 * Print a 1-bit image with GS v 0 (normal size). Verified on this unit:
 * prints 1:1, bit 7 = leftmost dot, not mirrored. (DC2 * and ESC * also
 * work; GS v 0 is used because it is the standard and allows tall images.)
 *
 * @param bits        row-major; each row is @p width_bytes bytes; in each
 *                    byte bit 7 is the leftmost dot; 1 = black
 * @param width_bytes dots per row / 8 (1..255 here)
 * @param height      rows (dots), 1..2047
 */
mc582h_status_t mc582h_print_raster(const uint8_t *bits, uint16_t width_bytes,
                                    uint16_t height);

/**
 * Ask for a real-time status byte (DLE EOT n). Uses no paper.
 * Measured on this unit: n=2 and n=3 follow the Epson format (0x12 idle,
 * bit 3 of n=2 set while FEED is held); n=1 returns 0xFE and n=4 returns
 * 0x92, i.e. NOT Epson's fixed bits, so only trust the documented flag
 * bits, not the "fixed" ones.
 *
 * @param[out] status the byte the printer returned
 */
mc582h_status_t mc582h_query_status(mc582h_status_class_t which,
                                    uint8_t *status);

#endif /* MC582H_H */
