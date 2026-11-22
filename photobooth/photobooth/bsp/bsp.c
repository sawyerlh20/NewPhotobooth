/**
 * @file    bsp.c
 * @brief   Board support implementation for the EK-TM4C123GXL.
 */
#include "bsp/bsp.h"
#include "bsp/board.h"

/* TODO: include the TivaWare headers you need. */

/* ---- Private data ---------------------------------------------------------
 * TODO: the uDMA channel control table (check the datasheet for its size
 * and alignment), and any state you need for button debouncing.          */

/* ---- Private helpers ------------------------------------------------------
 * TODO: e.g. button pin setup. Read the GPIO chapter of the datasheet
 * carefully for every pin you pick: some pins need extra steps.          */

/* ---- Public API ---------------------------------------------------------- */

/* TODO: your init function: system clock, LED(s), buttons. */

void bsp_delay_ms(uint32_t ms)
{
    /* TODO */
    (void)ms;
}

void bsp_delay_us(uint32_t us)
{
    /* TODO */
    (void)us;
}

void bsp_udma_init(void)
{
    /* TODO: enable the uDMA controller and point it at the channel control
     * table. Only do the work the first time it is called. */
}

/* TODO: LED and button functions. */

/* TODO (optional): debug UART init for UARTprintf(). */
