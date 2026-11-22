/**
 * @file    hm01b0_regs.h
 * @brief   HM01B0 register addresses (private to the driver).
 *
 * Names follow the Himax datasheet / OpenMV driver. Bit-field notes come
 * from OpenMV (drivers/sensors/hm01b0.c) and PicoHM01B0 (pmarques-dev),
 * which were verified on the same Arducam module.
 */
#ifndef HM01B0_REGS_H
#define HM01B0_REGS_H

/* Sensor ID */
#define HM01B0_REG_MODEL_ID_H          (0x0000U)
#define HM01B0_REG_MODEL_ID_L          (0x0001U)

/* Mode control */
#define HM01B0_REG_MODE_SELECT         (0x0100U) /* 0 standby, 1 streaming   */
#define HM01B0_REG_IMAGE_ORIENTATION   (0x0101U) /* [0] h-mirror [1] v-flip  */
#define HM01B0_REG_SW_RESET            (0x0103U)
#define HM01B0_REG_GRP_PARAM_HOLD      (0x0104U) /* write 1 to apply changes */

/* Frame timing */
#define HM01B0_REG_FRAME_LEN_LINES_H   (0x0340U)
#define HM01B0_REG_FRAME_LEN_LINES_L   (0x0341U)
#define HM01B0_REG_LINE_LEN_PCK_H      (0x0342U)
#define HM01B0_REG_LINE_LEN_PCK_L      (0x0343U)

/* Readout / binning */
#define HM01B0_REG_READOUT_X           (0x0383U)
#define HM01B0_REG_READOUT_Y           (0x0387U)
#define HM01B0_REG_BINNING_MODE        (0x0390U)

/* Test pattern */
#define HM01B0_REG_TEST_PATTERN_MODE   (0x0601U)

/* Black level / defect correction */
#define HM01B0_REG_BLC_CFG             (0x1000U)
#define HM01B0_REG_BLC_TGT             (0x1003U)
#define HM01B0_REG_BLI_EN              (0x1006U)
#define HM01B0_REG_BLC2_TGT            (0x1007U)
#define HM01B0_REG_DPC_CTRL            (0x1008U)
#define HM01B0_REG_VSYNC_HSYNC_SHIFT   (0x1012U)

/* Statistics / auto exposure */
#define HM01B0_REG_STATISTIC_CTRL      (0x2000U)
#define HM01B0_REG_AE_CTRL             (0x2100U) /* 1 = auto exposure on     */
#define HM01B0_REG_AE_TARGET_MEAN      (0x2101U)
#define HM01B0_REG_MAX_INTG_H          (0x2105U)
#define HM01B0_REG_MAX_INTG_L          (0x2106U)
#define HM01B0_REG_MD_CTRL             (0x2150U) /* motion detection         */

/* Interface / clocking */
#define HM01B0_REG_QVGA_WIN_EN         (0x3010U) /* [0] 1 = 324x244 window   */
#define HM01B0_REG_SIX_BIT_MODE_EN     (0x3011U)
#define HM01B0_REG_BIT_CONTROL         (0x3059U) /* bus width                */
#define HM01B0_REG_OSC_CLK_DIV         (0x3060U)
#define HM01B0_REG_CLK_SOURCE          (0x3067U) /* "ANA_Register_17"        */
#define HM01B0_REG_PCLK_POLARITY       (0x3068U)

/* ---- Register values ----------------------------------------------------- */

/* BIT_CONTROL: 0x02 = 8-bit, 0x42 = 4-bit (2 PCLK/pixel), 0x22 = 1-bit. */
#define HM01B0_BIT_CONTROL_4BIT        (0x42U)

/* OSC_CLK_DIV: [5] gated PCLK, [3] 0 = LSB (low nibble) first,
 * [1:0] vt_sys_div: 00 = MCLK/8, 01 = /4, 10 = /2, 11 = /1.
 * 0x20 = gated PCLK, low nibble first, MCLK/8 (slowest, easiest to poll). */
#define HM01B0_OSC_CLK_DIV_VALUE       (0x20U)

/* CLK_SOURCE: 0x00 = use external MCLK (XCLK pin), internal oscillator off. */
#define HM01B0_CLK_SOURCE_EXTERNAL     (0x00U)

/* PCLK_POLARITY: 0x20 = data valid on PCLK rising edge. */
#define HM01B0_PCLK_RISING             (0x20U)

#endif /* HM01B0_REGS_H */
