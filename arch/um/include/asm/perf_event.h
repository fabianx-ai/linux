/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_UM_PERF_EVENT_H
#define __ASM_UM_PERF_EVENT_H

/*
 * UML has no PMU; software/tracepoint events only. Every perf arch hook
 * has a generic default (include/linux/perf_event.h, kernel/events/), so
 * this header is intentionally empty. It must exist because UML's include
 * path contains arch/x86/include: without this shadow, <asm/perf_event.h>
 * would resolve to the real x86 header and its MSR/PMU externs.
 */

#endif /* __ASM_UM_PERF_EVENT_H */
