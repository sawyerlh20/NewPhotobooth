/**
 * @file    hm01b0.c
 * @brief   HM01B0 camera driver for the TM4C123 (see hm01b0.h).
 *
 * Sections
 *  1. Configuration constants and the register table
 *  2. I2C register access
 *  3. Hardware setup (I2C, GPIO)
 *  4. Pixel capture (the timing-critical part)
 *  5. Public API
 */
#include "drivers/hm01b0/hm01b0.h"
#include "drivers/hm01b0/hm01b0_regs.h"

#include <stddef.h>

#include "bsp/board.h"
#include "bsp/bsp.h"

/* TODO: include the TivaWare headers you need. */

/* ========================================================================== */
/* 1. Configuration                                                           */
/* ========================================================================== */

/**
 * Nibble order on the 4-bit bus. OSC_CLK_DIV bit 3 = 0 selects "LSB first":
 * the low nibble arrives first. Confirmed on hardware (a real scene is
 * smooth with 1, noise with 0).
 */
#define HM01B0_LOW_NIBBLE_FIRST     (1)

/*
 * Line and frame lengths, in pixels / lines. With PCLK ~6 MHz (2 edges per
 * pixel) one line period is LINE_LEN / 3 MHz:
 *   164x122: 215 -> 72 us per line, 176 lines -> 12.6 ms per frame (~80 fps)
 *   324x244: 376 -> 125 us per line, 260 lines -> 33 ms per frame (~30 fps)
 * A pixel is packed in the line AFTER it is captured, so packing a whole
 * line must finish within one HREF-high time (54 us / 108 us): easy.
 * Minimums measured by PicoHM01B0: 376 / 260 full, 215 / 172 binned.
 */
#define HM01B0_LINE_LEN_FULL        (376U)
#define HM01B0_FRAME_LEN_FULL       (260U)
#define HM01B0_LINE_LEN_BINNED      (215U)
#define HM01B0_FRAME_LEN_BINNED     (176U)

/** Loop iterations to wait for a sync edge (~10 cycles each, ~1 s total). */
#define HM01B0_SYNC_TIMEOUT_LOOPS   (8000000UL)

/** Loop iterations to wait for an I2C transfer (~100 kHz bus). */
#define HM01B0_I2C_TIMEOUT_LOOPS    (100000UL)

/** Extra attempts (each after a bus recovery) before an I2C call fails. */
#define HM01B0_I2C_RETRIES          (2U)

/** Time the sensor needs after a soft reset (longer than one frame). */
#define HM01B0_RESET_DELAY_MS       (200U)

/** One register write in an init table. */
typedef struct
{
    uint16_t reg;
    uint8_t  value;
} hm01b0_reg_entry_t;

/*
 * Common register settings. Image-quality values are Arducam's tuning for
 * this module (RPI-Pico-Cam/rp2040_hm01b0/arducam/hm01b0_init.h).
 * Interface values (bus width, clock source, polarity) are set for a
 * 4-bit bus - see hm01b0_regs.h.
 * Resolution-dependent registers are written separately (s_res_*).
 */
static const hm01b0_reg_entry_t s_init_regs[] =
{
    { HM01B0_REG_MODE_SELECT,      0x00U },  /* standby while configuring  */
    { HM01B0_REG_BLC_TGT,          0x08U },
    { HM01B0_REG_BLC2_TGT,         0x08U },
    { 0x3044U,                     0x0AU },  /* analog tuning (Himax)      */
    { 0x3045U,                     0x00U },
    { 0x3047U,                     0x0AU },
    { 0x3050U,                     0xC0U },
    { 0x3051U,                     0x42U },
    { 0x3052U,                     0x50U },
    { 0x3053U,                     0x00U },
    { 0x3054U,                     0x03U },
    { 0x3055U,                     0xF7U },
    { 0x3056U,                     0xF8U },
    { 0x3057U,                     0x29U },
    { 0x3058U,                     0x1FU },
    { 0x3064U,                     0x00U },
    { 0x3065U,                     0x04U },
    { HM01B0_REG_BLC_CFG,          0x43U },
    { 0x1001U,                     0x40U },
    { 0x1002U,                     0x32U },
    { 0x0350U,                     0x7FU },
    { HM01B0_REG_BLI_EN,           0x01U },
    { HM01B0_REG_DPC_CTRL,         0x00U },
    { 0x1009U,                     0xA0U },
    { 0x100AU,                     0x60U },
    { 0x100BU,                     0x90U },
    { 0x100CU,                     0x40U },
    { 0x3022U,                     0x01U },  /* advance VSYNC              */
    { HM01B0_REG_STATISTIC_CTRL,   0x07U },
    { 0x2003U,                     0x00U },  /* AE window                  */
    { 0x2004U,                     0x1CU },
    { 0x2007U,                     0x00U },
    { 0x2008U,                     0x58U },
    { 0x200BU,                     0x00U },
    { 0x200CU,                     0x7AU },
    { 0x200FU,                     0x00U },
    { 0x2010U,                     0xB8U },
    { 0x2013U,                     0x00U },
    { 0x2014U,                     0x58U },
    { 0x2017U,                     0x00U },
    { 0x2018U,                     0x9BU },
    { HM01B0_REG_AE_CTRL,          0x01U },  /* auto exposure on           */
    { HM01B0_REG_AE_TARGET_MEAN,   0x5FU },
    { 0x2102U,                     0x0AU },
    { 0x2103U,                     0x03U },
    { 0x2104U,                     0x05U },
    { 0x2107U,                     0x02U },
    { 0x2108U,                     0x03U },
    { 0x2109U,                     0x03U },
    { 0x210AU,                     0x00U },
    { 0x210BU,                     0x80U },
    { 0x210CU,                     0x40U },
    { 0x210DU,                     0x20U },
    { 0x210EU,                     0x03U },  /* flicker control            */
    { 0x210FU,                     0x00U },
    { 0x2110U,                     0x85U },
    { 0x2111U,                     0x00U },
    { 0x2112U,                     0xA0U },
    { HM01B0_REG_MD_CTRL,          0x00U },  /* motion detection off       */
    { HM01B0_REG_SIX_BIT_MODE_EN,  0x70U },  /* 8-bit pixels               */
    { HM01B0_REG_BIT_CONTROL,      HM01B0_BIT_CONTROL_4BIT },
    { HM01B0_REG_OSC_CLK_DIV,      HM01B0_OSC_CLK_DIV_VALUE },
    { HM01B0_REG_CLK_SOURCE,       HM01B0_CLK_SOURCE_EXTERNAL },
    { HM01B0_REG_PCLK_POLARITY,    HM01B0_PCLK_RISING },
    { HM01B0_REG_TEST_PATTERN_MODE, 0x00U }
};

#define HM01B0_INIT_REG_COUNT \
    ((uint32_t)(sizeof(s_init_regs) / sizeof(s_init_regs[0])))

/* ---- Driver state -------------------------------------------------------- */

static bool            s_bus_ready   = false;  /* I2C usable              */
static bool            s_initialised = false;  /* sensor configured       */
static hm01b0_config_t s_config;

/* ========================================================================== */
/* 2. I2C register access                                                     */
/* ========================================================================== */

/**
 * Wait for the current I2C operation to finish and check for errors.
 * @return HM01B0_OK, HM01B0_ERR_TIMEOUT or HM01B0_ERR_I2C
 */
static hm01b0_status_t i2c_wait(void)
{
    hm01b0_status_t status = HM01B0_ERR_TIMEOUT;

    /* TODO: wait for the I2C module to finish (give up after
     * HM01B0_I2C_TIMEOUT_LOOPS), then check whether it reported an error
     * (e.g. no ACK from the camera). */

    return status;
}

/**
 * Start a write to the camera and send the 16-bit register address, high
 * byte first. If keep_bus is true, do NOT end the transfer (a data byte
 * follows); otherwise finish it with a STOP.
 */
static hm01b0_status_t i2c_send_reg_addr(uint16_t reg, bool keep_bus)
{
    hm01b0_status_t status = HM01B0_ERR_I2C;

    /* TODO: use i2c_wait() after every step. If anything fails, end the
     * transfer cleanly so the next one can start. */
    (void)reg;
    (void)keep_bus;

    return status;
}

/**
 * Free a bus that a slave is holding.
 *
 * If SCL glitches (noise from the fast pixel-clock wires, or an MCU reset in
 * the middle of a transfer), the sensor can lose count of the bits and keep
 * SDA low while it waits for clocks that never come. The master then reports
 * "arbitration lost" on every transfer, forever (the camera is not reset when
 * the MCU is). The fix from the I2C spec (UM10204 sec. 3.1.16): clock SCL by
 * hand until the slave releases SDA (at most 9 clocks), then send a STOP.
 */
static void i2c_bus_recover(void)
{
    /* TODO (recommended once the basic I2C works): take the two pins away
     * from the I2C module, clock SCL by hand until SDA is released, send a
     * STOP, then give the pins back and re-initialise the I2C module. */
}

/** One write attempt, no recovery: register address, then one data byte. */
static hm01b0_status_t i2c_write_once(uint16_t reg, uint8_t value)
{
    hm01b0_status_t status = i2c_send_reg_addr(reg, true);

    if (status == HM01B0_OK)
    {
        /* TODO: send the data byte and finish the transfer. */
        (void)value;
        status = HM01B0_ERR_I2C;
    }
    return status;
}

/** One read attempt, no recovery: write the register address (with a
 *  STOP), then a separate 1-byte read. */
static hm01b0_status_t i2c_read_once(uint16_t reg, uint8_t *value)
{
    hm01b0_status_t status = i2c_send_reg_addr(reg, false);

    if (status == HM01B0_OK)
    {
        /* TODO: read one byte from the camera into *value. */
        (void)value;
        status = HM01B0_ERR_I2C;
    }
    return status;
}

hm01b0_status_t hm01b0_write_reg(uint16_t reg, uint8_t value)
{
    hm01b0_status_t status = HM01B0_ERR_NOT_INIT;
    uint32_t attempt;

    if (s_bus_ready)
    {
        status = i2c_write_once(reg, value);
        for (attempt = 0U; (attempt < HM01B0_I2C_RETRIES) &&
                           (status != HM01B0_OK); attempt++)
        {
            i2c_bus_recover();
            status = i2c_write_once(reg, value);
        }
    }
    return status;
}

hm01b0_status_t hm01b0_read_reg(uint16_t reg, uint8_t *value)
{
    hm01b0_status_t status = HM01B0_ERR_NOT_INIT;
    uint32_t attempt;

    if (value == NULL)
    {
        status = HM01B0_ERR_PARAM;
    }
    else if (s_bus_ready)
    {
        status = i2c_read_once(reg, value);
        for (attempt = 0U; (attempt < HM01B0_I2C_RETRIES) &&
                           (status != HM01B0_OK); attempt++)
        {
            i2c_bus_recover();
            status = i2c_read_once(reg, value);
        }
    }
    else
    {
        /* status already HM01B0_ERR_NOT_INIT */
    }
    return status;
}

/** Write a 16-bit value to a high/low register pair. */
static hm01b0_status_t write_reg16(uint16_t reg_high, uint16_t value)
{
    hm01b0_status_t status;

    status = hm01b0_write_reg(reg_high, (uint8_t)(value >> 8U));
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg((uint16_t)(reg_high + 1U),
                                  (uint8_t)(value & 0xFFU));
    }
    return status;
}

/* ========================================================================== */
/* 3. Hardware setup                                                          */
/* ========================================================================== */

/** Set up the camera's I2C module as a master, and its two pins. */
static void i2c_setup(void)
{
    /* TODO: peripheral clocks, pin muxing, pin types, and the I2C module
     * itself (the HM01B0 supports 100 kHz and 400 kHz). Pull-ups are
     * needed on SCL and SDA. Then free a possibly stuck bus with
     * i2c_bus_recover(). */
}

/** Configure the pixel bus (D0..D3, PCLK, HREF) and VSYNC pins as inputs. */
static void pixel_bus_setup(void)
{
    /* TODO */
}

/** Write the resolution-dependent registers and apply them. */
static hm01b0_status_t apply_resolution(const hm01b0_config_t *config)
{
    hm01b0_status_t status;
    uint16_t line_len;
    uint16_t frame_len;
    uint8_t  readout;
    uint8_t  binning;
    uint8_t  sync_shift;
    uint8_t  orientation = 0U;

    if (config->resolution == HM01B0_RES_164X122)
    {
        line_len   = HM01B0_LINE_LEN_BINNED;
        frame_len  = HM01B0_FRAME_LEN_BINNED;
        readout    = 0x03U;      /* skip every other pixel / line */
        binning    = 0x03U;      /* 2x2 binning                   */
        sync_shift = 0x03U;
    }
    else
    {
        line_len   = HM01B0_LINE_LEN_FULL;
        frame_len  = HM01B0_FRAME_LEN_FULL;
        readout    = 0x01U;
        binning    = 0x00U;
        sync_shift = 0x01U;
    }

    if (config->flip_horizontal)
    {
        orientation |= 0x01U;
    }
    if (config->flip_vertical)
    {
        orientation |= 0x02U;
    }

    status = write_reg16(HM01B0_REG_LINE_LEN_PCK_H, line_len);
    if (status == HM01B0_OK)
    {
        status = write_reg16(HM01B0_REG_FRAME_LEN_LINES_H, frame_len);
    }
    if (status == HM01B0_OK)
    {
        /* Exposure longer than (frame lines - 2) makes the sensor drop frames. */
        status = write_reg16(HM01B0_REG_MAX_INTG_H, (uint16_t)(frame_len - 2U));
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_QVGA_WIN_EN, 0x01U);
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_READOUT_X, readout);
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_READOUT_Y, readout);
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_BINNING_MODE, binning);
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_VSYNC_HSYNC_SHIFT, sync_shift);
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_IMAGE_ORIENTATION, orientation);
    }
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_GRP_PARAM_HOLD, 0x01U);
    }
    return status;
}

/* ========================================================================== */
/* 4. Pixel capture                                                           */
/* ========================================================================== */

/*
 * How a line is captured
 * ----------------------
 * PCLK is far too fast for the CPU to poll, so every PCLK edge must trigger
 * a uDMA transfer that copies D0..D3 into s_samples. The CPU only watches
 * HREF (line valid) and VSYNC (frame valid) to know where lines and frames
 * start and end.
 *
 *  - A line is turned into pixels during the NEXT line, while the DMA is
 *    idle. So one frame captures every other row, and a whole image takes
 *    two frames ("passes"): even rows, then odd rows.
 *  - Each line should give exactly 2 x width samples (2 nibbles per pixel).
 */

/** DMA transfer size: more than one line, so extra edges show up as errors. */
#define CAM_DMA_MAX         ((2U * HM01B0_FULL_WIDTH) + 16U)
/** Times a pass is repeated after a line with the wrong sample count. */
#define HM01B0_MAX_ATTEMPTS (3U)

/** DMA destination: one line of raw nibbles (one per byte). 664 bytes. */
static uint8_t s_samples[CAM_DMA_MAX];

static bool href_high(void)
{
    /* TODO: return the current level of the HREF pin. */
    return false;
}

static bool vsync_high(void)
{
    /* TODO: return the current level of the VSYNC pin. */
    return false;
}

/** (Optional, for hm01b0_measure) current CPU cycle count. */
static uint32_t cycle_count(void)
{
    /* TODO (optional): the Cortex-M4 has a cycle counter that can be read
     * here. Return 0 if you do not use it. */
    return 0U;
}

/** Route PCLK edges to a uDMA channel. Done once, at init. */
static void dma_trigger_setup(void)
{
    bsp_udma_init();

    /* TODO: pick the uDMA channel that the camera's GPIO port can trigger
     * (datasheet, uDMA channel assignments), configure it to copy one byte
     * per request from the pixel-bus port into an incrementing buffer, and
     * make the PCLK pin's edge trigger it. No interrupt handler should run. */
}

/** Start collecting samples into s_samples. Call while HREF is low. */
static void dma_arm(void)
{
    /* TODO: set up a transfer of up to CAM_DMA_MAX bytes into s_samples
     * and enable the channel. */
}

/** Stop the DMA. @return number of samples it collected. */
static uint32_t dma_stop(void)
{
    /* TODO */
    return 0U;
}

/**
 * Wait until HREF reads @p level.
 * @param dma_running the DMA is collecting samples while we wait
 * @param frame_over  if not NULL, also stop (and set it) when VSYNC falls
 */
static hm01b0_status_t wait_href(bool level, bool dma_running,
                                 bool *frame_over)
{
    hm01b0_status_t status = HM01B0_ERR_TIMEOUT;
    uint32_t loops = HM01B0_SYNC_TIMEOUT_LOOPS;
    bool waiting = true;

    if (frame_over != NULL)
    {
        *frame_over = false;
    }

    while (waiting && (loops > 0U))
    {
        if (href_high() == level)
        {
            waiting = false;
            status  = HM01B0_OK;
        }
        else if ((frame_over != NULL) && (!vsync_high()))
        {
            waiting     = false;
            *frame_over = true;
            status      = HM01B0_OK;
        }
        else
        {
            if (dma_running)
            {
                /* TODO: think about what this polling loop does to the
                 * DMA while it is running. */
            }
            loops--;
        }
    }
    return status;
}

/** Wait for the start of a frame: VSYNC low (blanking), then high. */
static hm01b0_status_t wait_frame_start(void)
{
    uint32_t loops = HM01B0_SYNC_TIMEOUT_LOOPS;

    while (vsync_high() && (loops > 0U))
    {
        loops--;
    }
    while ((!vsync_high()) && (loops > 0U))
    {
        loops--;
    }
    return (loops > 0U) ? HM01B0_OK : HM01B0_ERR_TIMEOUT;
}

/** Turn the samples of one line into @p width pixels. */
static void pack_line(uint8_t *dst, uint16_t width)
{
    uint32_t x;
    uint32_t first;
    uint32_t second;

    for (x = 0U; x < (uint32_t)width; x++)
    {
        first = (uint32_t)s_samples[(2U * x) + 1U] & 0x0FU;
        if ((x + 1U) < (uint32_t)width)
        {
            second = (uint32_t)s_samples[(2U * x) + 2U] & 0x0FU;
        }
        else
        {
            second = first;                  /* last nibble is not captured */
        }
#if (HM01B0_LOW_NIBBLE_FIRST == 1)
        dst[x] = (uint8_t)((second << 4U) | first);
#else
        dst[x] = (uint8_t)((first << 4U) | second);
#endif
    }
}

/** What capture_frame() should collect. */
typedef struct
{
    uint16_t first_row;   /* first row to keep                               */
    uint16_t row_count;   /* rows to keep                                    */
    uint8_t *buffer;      /* row-major, row first_row at offset 0; NULL =
                             only count samples (hm01b0_measure)             */
    bool     full_frame;  /* keep going to the end of the frame              */
} capture_job_t;

/** Rows captured in pass @p pass: every other row, starting at first+pass. */
static bool row_wanted(const capture_job_t *job, uint32_t row, uint16_t pass)
{
    bool wanted = false;

    if ((row >= (uint32_t)job->first_row) &&
        (row < ((uint32_t)job->first_row + (uint32_t)job->row_count)))
    {
        wanted = (((row - (uint32_t)job->first_row) % 2U) == (uint32_t)pass);
    }
    return wanted;
}

/**
 * Capture one frame: rows of @p pass are DMA'd, then packed during the
 * following (not captured) line. Interrupts are off for up to one frame
 * (13 ms at 164x122, 33 ms at 324x244) so that line counting stays exact.
 */
static hm01b0_status_t capture_frame(const capture_job_t *job, uint16_t pass,
                                     hm01b0_frame_info_t *info)
{
    hm01b0_status_t status;
    const uint16_t width    = hm01b0_width();
    const uint32_t expected = 2U * (uint32_t)width;
    const uint32_t last_row = (uint32_t)job->first_row + (uint32_t)job->row_count;
    uint32_t row = 0U;
    uint32_t pending_row = 0U;
    uint32_t samples;
    uint32_t start = 0U;
    bool pending    = false;      /* a captured line waits to be packed     */
    bool bad_line   = false;      /* a line had the wrong sample count      */
    bool armed;
    bool frame_over = false;
    bool done       = false;

    info->lines_per_frame = 0U;
    info->min_pixels  = 0xFFFFU;
    info->max_pixels  = 0U;
    info->line_cycles = 0U;
    info->pclk_edges  = 0U;

    /* TODO: no interrupt may delay this loop until the frame is done. */

    status = wait_frame_start();
    while ((status == HM01B0_OK) && (!done))
    {
        /* Here HREF is low: the gap before line 'row'. */
        armed = row_wanted(job, row, pass);
        if (armed)
        {
            dma_arm();
        }

        status = wait_href(true, armed, &frame_over);
        if ((status != HM01B0_OK) || frame_over)
        {
            if (armed)
            {
                (void)dma_stop();
            }
            done = true;
        }
        else
        {
            start = cycle_count();

            /* The DMA is idle on this line (never two rows in a row), so
             * this is the time to pack the previous one. */
            if (pending)
            {
                pack_line(&job->buffer[(pending_row - (uint32_t)job->first_row) *
                                       (uint32_t)width], width);
                pending = false;
            }

            status = wait_href(false, armed, NULL);

            if (armed && (status == HM01B0_OK))
            {
                samples = dma_stop();
                if (info->line_cycles == 0U)
                {
                    info->line_cycles = cycle_count() - start;
                    info->pclk_edges  = samples;
                }
                if ((samples / 2U) < (uint32_t)info->min_pixels)
                {
                    info->min_pixels = (uint16_t)(samples / 2U);
                }
                if ((samples / 2U) > (uint32_t)info->max_pixels)
                {
                    info->max_pixels = (uint16_t)(samples / 2U);
                }

                if (samples != expected)
                {
                    bad_line = true;         /* finish the frame, then report */
                }
                else if (job->buffer != NULL)
                {
                    pending     = true;
                    pending_row = row;
                }
                else
                {
                    /* measuring only */
                }
            }
            info->lines_per_frame++;
            row++;
            if ((!job->full_frame) && (row >= last_row))
            {
                done = true;                     /* got every row we wanted */
            }
        }
    }

    if ((status == HM01B0_OK) && pending)
    {
        pack_line(&job->buffer[(pending_row - (uint32_t)job->first_row) *
                               (uint32_t)width], width);
    }

    /* TODO: undo what you did to interrupts above (leave them as they
     * were before this function was called). */

    if (info->min_pixels == 0xFFFFU)
    {
        info->min_pixels = 0U;
    }
    if ((status == HM01B0_OK) && (row < last_row))
    {
        status = HM01B0_ERR_TIMEOUT;             /* frame ended too early */
    }
    if ((status == HM01B0_OK) && bad_line)
    {
        status = HM01B0_ERR_DATA;
    }
    return status;
}

/** Run every pass of @p job, repeating a pass that saw a bad line. */
static hm01b0_status_t capture_job(const capture_job_t *job,
                                   hm01b0_frame_info_t *info)
{
    hm01b0_status_t status = HM01B0_OK;
    const uint16_t passes = (job->row_count > 1U) ? 2U : 1U;
    uint16_t pass;
    uint32_t attempt;

    for (pass = 0U; (pass < passes) && (status == HM01B0_OK); pass++)
    {
        attempt = 0U;
        do
        {
            status = capture_frame(job, pass, info);
            attempt++;
        } while ((status == HM01B0_ERR_DATA) &&
                 (attempt < HM01B0_MAX_ATTEMPTS));
    }
    return status;
}

/* ========================================================================== */
/* 5. Public API                                                              */
/* ========================================================================== */

hm01b0_status_t hm01b0_read_model_id(uint16_t *id)
{
    hm01b0_status_t status;
    uint8_t high = 0U;
    uint8_t low  = 0U;

    if (id == NULL)
    {
        status = HM01B0_ERR_PARAM;
    }
    else
    {
        status = hm01b0_read_reg(HM01B0_REG_MODEL_ID_H, &high);
        if (status == HM01B0_OK)
        {
            status = hm01b0_read_reg(HM01B0_REG_MODEL_ID_L, &low);
        }
        if (status == HM01B0_OK)
        {
            *id = (uint16_t)(((uint16_t)high << 8U) | (uint16_t)low);
        }
    }
    return status;
}

hm01b0_status_t hm01b0_init(const hm01b0_config_t *config)
{
    hm01b0_status_t status = HM01B0_OK;
    uint16_t id = 0U;
    uint32_t i;

    if (config == NULL)
    {
        status = HM01B0_ERR_PARAM;
    }
    else
    {
        s_initialised = false;
        s_config = *config;

        i2c_setup();
        pixel_bus_setup();
        dma_trigger_setup();
        s_bus_ready = true;

        /* Check that the sensor answers before doing anything else. */
        status = hm01b0_read_model_id(&id);
        if ((status == HM01B0_OK) && (id != HM01B0_MODEL_ID))
        {
            status = HM01B0_ERR_ID;
        }

        /* Soft reset (value as used by Arducam/PicoHM01B0 for this module). */
        if (status == HM01B0_OK)
        {
            status = hm01b0_write_reg(HM01B0_REG_SW_RESET, 0x00U);
            bsp_delay_ms(HM01B0_RESET_DELAY_MS);
        }

        for (i = 0U; (i < HM01B0_INIT_REG_COUNT) && (status == HM01B0_OK); i++)
        {
            status = hm01b0_write_reg(s_init_regs[i].reg, s_init_regs[i].value);
        }

        if (status == HM01B0_OK)
        {
            status = apply_resolution(&s_config);
        }
        if (status == HM01B0_OK)
        {
            status = hm01b0_write_reg(HM01B0_REG_MODE_SELECT, 0x01U); /* stream */
        }
        if (status == HM01B0_OK)
        {
            s_initialised = true;
        }
    }
    return status;
}

hm01b0_status_t hm01b0_set_test_pattern(hm01b0_test_pattern_t pattern)
{
    hm01b0_status_t status = hm01b0_write_reg(HM01B0_REG_TEST_PATTERN_MODE,
                                              (uint8_t)pattern);
    if (status == HM01B0_OK)
    {
        status = hm01b0_write_reg(HM01B0_REG_GRP_PARAM_HOLD, 0x01U);
    }
    return status;
}

uint16_t hm01b0_width(void)
{
    return (s_config.resolution == HM01B0_RES_164X122) ?
           (uint16_t)HM01B0_BINNED_WIDTH : (uint16_t)HM01B0_FULL_WIDTH;
}

uint16_t hm01b0_height(void)
{
    return (s_config.resolution == HM01B0_RES_164X122) ?
           (uint16_t)HM01B0_BINNED_HEIGHT : (uint16_t)HM01B0_FULL_HEIGHT;
}

hm01b0_status_t hm01b0_measure(hm01b0_frame_info_t *info)
{
    hm01b0_status_t status = HM01B0_ERR_NOT_INIT;
    capture_job_t job = { 0U, 0U, NULL, true };

    if (info == NULL)
    {
        status = HM01B0_ERR_PARAM;
    }
    else if (s_initialised)
    {
        job.row_count = hm01b0_height();
        status = capture_frame(&job, 0U, info);   /* one frame, no storage  */
    }
    else
    {
        /* not initialised */
    }
    return status;
}

hm01b0_status_t hm01b0_capture_rows(uint16_t first_row, uint16_t row_count,
                                    uint8_t *buffer, uint32_t buffer_size)
{
    hm01b0_status_t status = HM01B0_ERR_NOT_INIT;
    hm01b0_frame_info_t info;
    capture_job_t job = { 0U, 0U, NULL, false };

    if ((buffer == NULL) || (row_count == 0U) ||
        (((uint32_t)first_row + (uint32_t)row_count) > (uint32_t)hm01b0_height()))
    {
        status = HM01B0_ERR_PARAM;
    }
    else if (buffer_size < ((uint32_t)row_count * (uint32_t)hm01b0_width()))
    {
        status = HM01B0_ERR_BUFFER;
    }
    else if (s_initialised)
    {
        job.first_row = first_row;
        job.row_count = row_count;
        job.buffer    = buffer;
        status = capture_job(&job, &info);
    }
    else
    {
        /* not initialised */
    }
    return status;
}

hm01b0_status_t hm01b0_capture_lines(uint8_t *line_buffer,
                                     uint32_t line_buffer_size,
                                     hm01b0_line_cb_t callback,
                                     void *context)
{
    hm01b0_status_t status = HM01B0_ERR_NOT_INIT;
    const uint16_t height = hm01b0_height();
    const uint16_t width  = hm01b0_width();
    uint16_t row;

    if ((line_buffer == NULL) || (callback == NULL))
    {
        status = HM01B0_ERR_PARAM;
    }
    else if (line_buffer_size < (uint32_t)width)
    {
        status = HM01B0_ERR_BUFFER;
    }
    else if (s_initialised)
    {
        /* One row per frame: the callback runs with interrupts on and may
         * take as long as it needs (the next row comes from a later frame). */
        status = HM01B0_OK;
        for (row = 0U; (row < height) && (status == HM01B0_OK); row++)
        {
            status = hm01b0_capture_rows(row, 1U, line_buffer, line_buffer_size);
            if ((status == HM01B0_OK) &&
                (!callback(row, line_buffer, width, context)))
            {
                status = HM01B0_ERR_CALLBACK;
            }
        }
    }
    else
    {
        /* not initialised */
    }
    return status;
}
