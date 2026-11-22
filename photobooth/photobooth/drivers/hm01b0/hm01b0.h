/**
 * @file    hm01b0.h
 * @brief   Driver for the Himax HM01B0 monochrome camera
 *          (Arducam "HM01B0 QVGA SPI Camera Module for Raspberry Pi Pico").
 *
 * Hardware interface
 * ------------------
 *  - Control: I2C, 7-bit address 0x24, 16-bit register addresses, 8-bit data.
 *  - Clock:   the module has its own oscillator on XCLK.
 *  - Pixels:  4-bit parallel bus (D0..D3), low nibble first. PCLK only
 *             toggles while HREF (line valid) is high. It is too fast to
 *             poll, so each PCLK edge triggers a uDMA transfer of D0..D3
 *             into SRAM. Uses the shared uDMA table from bsp_udma_init().
 *
 * Memory model
 * ------------
 *  The driver never allocates memory. The caller supplies every buffer, and
 *  chooses how much of the frame to keep:
 *   - hm01b0_capture_rows() with row_count = full height: a whole frame
 *     (only practical for HM01B0_RES_164X122, 20 kB). Takes 2 frames.
 *   - hm01b0_capture_rows() with a band of rows: e.g. 40 rows of 324 px
 *     (13 kB) at a time, to process a full-resolution image in strips.
 *   - hm01b0_capture_lines(): one row per frame handed to a callback, so
 *     only one line (324 bytes) is ever in RAM. Slow (~1.5 s / ~8 s).
 *  Internally the driver also uses a 664-byte DMA sample buffer.
 *
 * Timing: interrupts are disabled while a frame is captured (13 ms at
 * 164x122, 33 ms at 324x244), so the line count cannot be disturbed.
 * Rows from different frames are combined, so fast motion can tear.
 */
#ifndef HM01B0_H
#define HM01B0_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Constants ----------------------------------------------------------- */

#define HM01B0_I2C_ADDR       (0x24U)    /**< 7-bit I2C address              */
#define HM01B0_MODEL_ID       (0x01B0U)  /**< value of MODEL_ID_H:MODEL_ID_L */

#define HM01B0_FULL_WIDTH     (324U)     /**< QVGA window incl. 2 px border  */
#define HM01B0_FULL_HEIGHT    (244U)
#define HM01B0_BINNED_WIDTH   (164U)     /**< 2x2 binned                     */
#define HM01B0_BINNED_HEIGHT  (122U)

/* ---- Types --------------------------------------------------------------- */

/** Return code of every driver function. */
typedef enum
{
    HM01B0_OK = 0,          /**< success                                     */
    HM01B0_ERR_PARAM,       /**< NULL pointer or out-of-range argument       */
    HM01B0_ERR_NOT_INIT,    /**< hm01b0_init() has not succeeded yet         */
    HM01B0_ERR_I2C,         /**< I2C NAK or bus error (check wiring/pull-ups)*/
    HM01B0_ERR_ID,          /**< chip answered but MODEL_ID != 0x01B0        */
    HM01B0_ERR_TIMEOUT,     /**< no VSYNC/HREF activity (check XCLK, wiring) */
    HM01B0_ERR_BUFFER,      /**< caller's buffer is too small                */
    HM01B0_ERR_CALLBACK,    /**< line callback asked to stop                 */
    HM01B0_ERR_DATA         /**< line had the wrong number of PCLK edges,
                                 even after retries (noise on PCLK?)         */
} hm01b0_status_t;

/** Output resolution. */
typedef enum
{
    HM01B0_RES_324X244 = 0, /**< QVGA window: 77 kB per frame, NOT storable  */
    HM01B0_RES_164X122      /**< 2x2 binning: 20 kB per frame, fits in SRAM  */
} hm01b0_resolution_t;

/** Built-in test patterns (register TEST_PATTERN_MODE). */
typedef enum
{
    HM01B0_TEST_OFF          = 0x00U, /**< normal image                      */
    HM01B0_TEST_COLOR_BAR    = 0x01U, /**< vertical grey bars                */
    HM01B0_TEST_WALKING_ONES = 0x11U  /**< 1,2,4..128 repeating per pixel    */
} hm01b0_test_pattern_t;

/** Driver configuration, passed once to hm01b0_init(). */
typedef struct
{
    hm01b0_resolution_t resolution;
    bool                flip_horizontal;
    bool                flip_vertical;
} hm01b0_config_t;

/** Frame geometry actually measured on the wire (see hm01b0_measure()). */
typedef struct
{
    uint16_t lines_per_frame;   /**< HREF pulses between VSYNC edges         */
    uint16_t min_pixels;        /**< shortest sampled line, in pixels        */
    uint16_t max_pixels;        /**< longest sampled line, in pixels         */
    uint32_t line_cycles;       /**< CPU cycles HREF stays high on one line  */
    uint32_t pclk_edges;        /**< PCLK edges (DMA samples) on it          */
} hm01b0_frame_info_t;

/**
 * Called once per captured line by hm01b0_capture_lines().
 *
 * @param row     line number, 0 = top
 * @param pixels  the line's pixels (valid only during the call)
 * @param width   number of pixels in @p pixels
 * @param context caller's pointer, passed through unchanged
 * @return true to continue, false to abort the capture
 *
 * Runs with interrupts enabled, after the line has been captured. It may
 * take as long as it needs: the next row is taken from a later frame.
 */
typedef bool (*hm01b0_line_cb_t)(uint16_t row, const uint8_t *pixels,
                                 uint16_t width, void *context);

/* ---- API ----------------------------------------------------------------- */

/**
 * Set up I2C (freeing a stuck bus), the pixel-bus GPIOs and the PCLK->uDMA
 * trigger, reset the sensor, check its ID and load the register table.
 * Leaves the sensor streaming.
 */
hm01b0_status_t hm01b0_init(const hm01b0_config_t *config);

/** Read the 16-bit model ID (expected HM01B0_MODEL_ID). Needs only I2C. */
hm01b0_status_t hm01b0_read_model_id(uint16_t *id);

/** Write one sensor register. */
hm01b0_status_t hm01b0_write_reg(uint16_t reg, uint8_t value);

/** Read one sensor register. */
hm01b0_status_t hm01b0_read_reg(uint16_t reg, uint8_t *value);

/** Enable a built-in test pattern, or HM01B0_TEST_OFF for the real image. */
hm01b0_status_t hm01b0_set_test_pattern(hm01b0_test_pattern_t pattern);

/** Width/height of the configured resolution. */
uint16_t hm01b0_width(void);
uint16_t hm01b0_height(void);

/**
 * Count the lines of one frame and the pixels of every other line (DMA),
 * without storing anything. Use this first: if it times out there is no
 * VSYNC/HREF/PCLK; if min/max pixels differ from hm01b0_width(), PCLK
 * edges are missed or extra (wiring/noise). Returns HM01B0_ERR_DATA in that
 * case, with @p info still filled in.
 */
hm01b0_status_t hm01b0_measure(hm01b0_frame_info_t *info);

/**
 * Capture rows [first_row, first_row + row_count) into @p buffer
 * (row-major, hm01b0_width() bytes per row). Even rows come from one frame
 * and odd rows from the next, so it takes 2 frames (1 if row_count == 1).
 *
 * @param buffer_size size of @p buffer in bytes;
 *                    must be >= row_count * hm01b0_width()
 */
hm01b0_status_t hm01b0_capture_rows(uint16_t first_row, uint16_t row_count,
                                    uint8_t *buffer, uint32_t buffer_size);

/**
 * Capture a whole image one row per frame. Each row is written to
 * @p line_buffer and then passed to @p callback.
 *
 * @param line_buffer_size must be >= hm01b0_width()
 */
hm01b0_status_t hm01b0_capture_lines(uint8_t *line_buffer,
                                     uint32_t line_buffer_size,
                                     hm01b0_line_cb_t callback,
                                     void *context);

#endif /* HM01B0_H */
