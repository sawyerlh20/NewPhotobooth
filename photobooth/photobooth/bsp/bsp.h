/**
 * @file    bsp.h
 * @brief   Board support: everything that is about the LaunchPad itself
 *          (clock, delays, LED, buttons, debug UART, shared uDMA setup).
 *
 * Drivers build on top of this layer. The three functions declared below
 * are already called by the provided drivers, so keep their names and
 * signatures. Everything else here is up to you.
 *
 * TODO: declare the rest of your board-support API. It should cover:
 *  - an init function that sets the system clock and sets up the LED(s)
 *    and the two on-board buttons (call it first in main)
 *  - turning the LED(s) on and off
 *  - reading the buttons: the raw state, and a debounced "was pressed"
 *    check that reports each press only once
 *  - (optional) setting up the debug UART so UARTprintf() from
 *    utils/uartstdio works
 */
#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>

/** Busy-wait for the given number of milliseconds. */
void bsp_delay_ms(uint32_t ms);

/** Busy-wait for the given number of microseconds. */
void bsp_delay_us(uint32_t us);

/**
 * Enable the uDMA controller and give it its channel control table.
 * Safe to call more than once; every driver that uses DMA calls it from its
 * own init, and the table is shared (drivers must use different channels).
 */
void bsp_udma_init(void);

#endif /* BSP_H */
