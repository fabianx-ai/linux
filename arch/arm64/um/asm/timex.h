/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __UM_TIMEX_H
#define __UM_TIMEX_H

/*
 * The native asm/timex.h reads the architected counter through the
 * arch_timer driver, which a UML guest neither links nor may touch;
 * the generic fallback (get_cycles() == 0, entropy from the fallback
 * path) is what arch/um's own timex.h provided before mainline
 * removed it together with CLOCK_TICK_RATE.
 */
#include <asm-generic/timex.h>

#endif
