/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The stub execution contract for the arm64 UML backend:
 * svc #0 syscall sequences (x8 = nr, x0..x5 = args), brk #0 as the
 * trap-back notifier, adrp-based stub_data locator, stub_start stack
 * switch, and in-stub TLS restore via direct msr TPIDR_EL0
 * (EL0-writable — no arch_prctl round trip needed).
 */
#ifndef __SYSDEP_STUB_H
#define __SYSDEP_STUB_H

#include <stddef.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <as-layout.h>
#include <stub-data.h>
#include <sysdep/ptrace_user.h>
#include <generated/asm-offsets.h>
#include <linux/stddef.h>

#define STUB_MMAP_NR __NR_mmap

/*
 * TLS restore: arm64 has no native set_tls syscall — TPIDR_EL0 is
 * EL0-writable and the stub restores it via msr directly. This value
 * (the compat set_tls number) only keeps the filter table's shape.
 */
#define STUB_TLS_SYSCALL_NR (0x0f0000 + 5)
#define MMAP_OFFSET(o) (o)

#define __syscall_clobber "memory"

static __always_inline long stub_syscall0(long syscall)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0");

	__asm__ volatile ("svc #0"
		: "=r" (x0)
		: "r" (x8) : __syscall_clobber );

	return x0;
}

static __always_inline long stub_syscall1(long syscall, long arg1)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0") = arg1;

	__asm__ volatile ("svc #0"
		: "+r" (x0)
		: "r" (x8) : __syscall_clobber );

	return x0;
}

static __always_inline long stub_syscall2(long syscall, long arg1, long arg2)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0") = arg1;
	register long x1 __asm__("x1") = arg2;

	__asm__ volatile ("svc #0"
		: "+r" (x0)
		: "r" (x8), "r" (x1) : __syscall_clobber );

	return x0;
}

static __always_inline long stub_syscall3(long syscall, long arg1, long arg2,
					  long arg3)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0") = arg1;
	register long x1 __asm__("x1") = arg2;
	register long x2 __asm__("x2") = arg3;

	__asm__ volatile ("svc #0"
		: "+r" (x0)
		: "r" (x8), "r" (x1), "r" (x2) : __syscall_clobber );

	return x0;
}

static __always_inline long stub_syscall4(long syscall, long arg1, long arg2,
					  long arg3, long arg4)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0") = arg1;
	register long x1 __asm__("x1") = arg2;
	register long x2 __asm__("x2") = arg3;
	register long x3 __asm__("x3") = arg4;

	__asm__ volatile ("svc #0"
		: "+r" (x0)
		: "r" (x8), "r" (x1), "r" (x2), "r" (x3)
		: __syscall_clobber );

	return x0;
}

static __always_inline long stub_syscall5(long syscall, long arg1, long arg2,
					  long arg3, long arg4, long arg5)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0") = arg1;
	register long x1 __asm__("x1") = arg2;
	register long x2 __asm__("x2") = arg3;
	register long x3 __asm__("x3") = arg4;
	register long x4 __asm__("x4") = arg5;

	__asm__ volatile ("svc #0"
		: "+r" (x0)
		: "r" (x8), "r" (x1), "r" (x2), "r" (x3), "r" (x4)
		: __syscall_clobber );

	return x0;
}

static __always_inline long stub_syscall6(long syscall, long arg1, long arg2,
					  long arg3, long arg4, long arg5,
					  long arg6)
{
	register long x8 __asm__("x8") = syscall;
	register long x0 __asm__("x0") = arg1;
	register long x1 __asm__("x1") = arg2;
	register long x2 __asm__("x2") = arg3;
	register long x3 __asm__("x3") = arg4;
	register long x4 __asm__("x4") = arg5;
	register long x5 __asm__("x5") = arg6;

	__asm__ volatile ("svc #0"
		: "+r" (x0)
		: "r" (x8), "r" (x1), "r" (x2), "r" (x3), "r" (x4), "r" (x5)
		: __syscall_clobber );

	return x0;
}

static __always_inline void trap_myself(void)
{
	__asm("brk #0");
}

/*
 * The stub data page sits directly above the stub code page; adrp
 * yields the current page, add one page size.
 */
static __always_inline void *get_stub_data(void)
{
	unsigned long ret;

	asm volatile (
		"adrp %0, 0 ;"
		"add %0, %0, %1 ;"
		: "=&r" (ret)
		: "r" (UM_KERN_PAGE_SIZE));

	return (void *)ret;
}

#define stub_start(fn)							\
	asm volatile (							\
		"sub sp, sp, %0 ;"					\
		"adr x9, %1 ;"						\
		"blr x9 ;"						\
		:: "i" (STUB_SIZE),					\
		   "i" (&fn) : "x9")

static __always_inline void
stub_seccomp_restore_state(struct stub_data_arch *arch)
{
	/* TPIDR_EL0 is writable at EL0 — no syscall needed */
	if (arch->sync & STUB_SYNC_TLS)
		asm volatile ("msr TPIDR_EL0, %0" :: "r" (arch->tls));

	arch->sync = 0;
}

extern void stub_segv_handler(int, siginfo_t *, void *);
extern void stub_syscall_handler(void);
extern void stub_signal_interrupt(int, siginfo_t *, void *);
extern void stub_signal_restorer(void);

#endif
