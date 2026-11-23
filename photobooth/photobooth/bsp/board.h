/**
 * @file    board.h
 * @brief   EK-TM4C123GXL pin assignments for the photobooth project.
 *
 * Every pin used by the project is listed here, in one place, so that pin
 * conflicts are easy to spot. Drivers include this file instead of
 * hard-coding port/pin numbers.
 *
 * TODO: choose a pin for every signal below, fill in the table, and give
 * every macro in this file a value. Check each choice against the LaunchPad
 * user's guide and the TM4C123GH6PM datasheet (pin mux table, GPIO chapter).
 * Add, rename or remove macros as your design needs.
 *
 * Pin map
 * -------
 *  Signal        TM4C pin   Notes
 *  ------------  ---------  -----------------------------------------------
 *  Debug UART TX
 *  Debug UART RX
 *  CAM SCL
 *  CAM SDA
 *  CAM D0..D3
 *  CAM PCLK
 *  CAM HREF
 *  CAM VSYNC
 *  Red LED
 *  Printer RX
 *  Printer TX
 *  TFT SCK
 *  TFT MOSI
 *  TFT CS
 *  TFT D/C
 *  TFT RESET
 *  Button SW1
 *  Button SW2
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"

/* ---- System -------------------------------------------------------------- */
#define BOARD_SYSCLK_HZ           80_000_000_000

/* ---- Debug UART (over the LaunchPad's USB connection) -------------------- */
#define BOARD_DEBUG_UART_BAUD     115200

/* ---- Camera: I2C control bus --------------------------------------------- */
#define BOARD_CAM_I2C_PERIPH      SYSCTL_PERIPH_I2C1
#define BOARD_CAM_I2C_BASE        I2C1_BASE
#define BOARD_CAM_I2C_GPIO_PERIPH SYSCTL_PERIPH_GPIOB
#define BOARD_CAM_I2C_GPIO_BASE   GPIO_PORTB_BASE
#define BOARD_CAM_I2C_SCL_PIN     GPIO_PIN_2
#define BOARD_CAM_I2C_SDA_PIN     GPIO_PIN_3
#define BOARD_CAM_I2C_SCL_MUX     GPIO_PB2_I2C0SCL
#define BOARD_CAM_I2C_SDA_MUX     GPIO_PB3_I2C0SDA

/* ---- Camera: pixel bus ---------------------------------------------------
 * Hint: the camera driver reads D0..D3 as the low 4 bits of ONE GPIO port,
 * and PCLK must be on a pin whose port can trigger a uDMA transfer.        */
#define BOARD_CAM_DATA_PERIPH     SYSCTL_PERIPH_GPIOE
#define BOARD_CAM_DATA_BASE       GPIO_PORTE_BASEE
#define BOARD_CAM_D_MASK          0xF
#define BOARD_CAM_PCLK_PIN        GPIO_PIN_4
#define BOARD_CAM_HREF_PIN        GPIO_PIN_5

/* ---- Camera: frame sync -------------------------------------------------- */
#define BOARD_CAM_VSYNC_PERIPH    SYSCTL_PERIPH_GPIOF
#define BOARD_CAM_VSYNC_BASE      GPIO_PORTF_BASE
#define BOARD_CAM_VSYNC_PIN       GPIO_PIN_4

/* ---- Printer: MC582H (UART) ---------------------------------------------- */
#define BOARD_PRN_UART_PERIPH     SYSCTL_PERIPH_UART1
#define BOARD_PRN_UART_BASE       UART1_BASE
#define BOARD_PRN_GPIO_PERIPH     SYSCTL_PERIPH_GPIOB
#define BOARD_PRN_GPIO_BASE       GPIO_PORTB_BASE
#define BOARD_PRN_RX_PIN          GPIO_PIN_0
#define BOARD_PRN_TX_PIN          GPIO_PIN_1
#define BOARD_PRN_RX_MUX          GPIO_PB0_U1RX
#define BOARD_PRN_TX_MUX          GPIO_PB1_U1TX
#define BOARD_PRN_BAUD            115200

/* ---- Display: Adafruit 1.8" 160x128 TFT (ST7735R, SPI) ------------------- */
#define BOARD_TFT_SSI_PERIPH      SYSCTL_PERIPH_SSI1
#define BOARD_TFT_SSI_BASE        SSI1_BASE
#define BOARD_TFT_SPI_PERIPH      SYSCTL_PERIPH_SSI1
#define BOARD_TFT_SPI_BASE        SSI1_BASE
#define BOARD_TFT_SCK_PIN         GPIO_PIN_2
#define BOARD_TFT_MOSI_PIN        /* TODO */
#define BOARD_TFT_SCK_MUX         GPIO_PF2_SSI1CLK
#define BOARD_TFT_MOSI_MUX        /* TODO */
#define BOARD_TFT_CS_PIN          /* TODO */
#define BOARD_TFT_DC_PIN          /* TODO */
#define BOARD_TFT_RST_PERIPH      /* TODO */
#define BOARD_TFT_RST_BASE        /* TODO */
#define BOARD_TFT_RST_PIN         /* TODO */
#define BOARD_TFT_SPI_HZ          /* TODO */

/* ---- Buttons (on-board SW1/SW2) ------------------------------------------ */
#define BOARD_BTN_PERIPH          SYSCTL_PERIPH_GPIOE
#define BOARD_BTN_BASE            GPIO_PORTE_BASE
#define BOARD_BTN_SW1_PIN         GPIO_PIN_2
#define BOARD_BTN_SW2_PIN         GPIO_PIN_3

/* ---- LEDs ---------------------------------------------------------------- */
#define BOARD_LED_PERIPH          SYSCTL_PERIPH_GPIOA
#define BOARD_LED_BASE            GPIO_PORTA_BASE
#define BOARD_LED_RED_PIN         GPIO_PIN_0

#endif /* BOARD_H */
