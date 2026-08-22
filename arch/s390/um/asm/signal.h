/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Signal ABI for the s390x UML backend: the generic uapi definitions
 * plus the identity untagged-address helper (no top-byte-ignore on
 * s390). Shadows the native asm/signal.h, whose _NSIG/_SIGCONTEXT
 * chain is fine but which we keep out of the backend surface to stay
 * explicit about the guest ABI (64 signals, 64-bit mask words).
 */
#ifndef __UM_S390_SIGNAL_H
#define __UM_S390_SIGNAL_H

#include <asm-generic/signal.h>
#include <uapi/asm/siginfo.h>

static inline void __user *arch_untagged_si_addr(void __user *addr,
						 unsigned long sig,
						 unsigned long si_code)
{
	return addr;
}
#define arch_untagged_si_addr arch_untagged_si_addr

#endif
