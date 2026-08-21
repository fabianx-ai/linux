/* SPDX-License-Identifier: GPL-2.0 */
/*
 * The stub execution contract for the s390x UML backend:
 * `svc 0` with the syscall number in r1 (handles nr > 255; same shape
 * as native s390's GENERATE_SYSCALL_FUNC), trap-back via a 2-byte
 * ill-legal breakpoint (`j .+2` is wrong — use .insn or an illegal
 * halfword pair that SIGILLs), larl-based stub_data locator, stub
 * stack switch, and NO stub-carried TLS: acrs are ptrace-visible on
 * both consumption routes (F-s1), so save/restore state are no-ops.
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
 * s390 HAS SA_RESTORER (0x04000000): signal return goes through the
 * restorer trampoline in the stub page (svc __NR_rt_sigreturn passes
 * the filter's IP check) — x86 model, not arm64's vdso dance. [§5]
 */
#define UM_SA_RESTORER 0x04000000

/*
 * No in-stub TLS restore is needed: acrs[0..1] carry TLS and survive
 * traps intact (F-s1); the mcontext round-trip restores them. The
 * filter's TLS entry is vestigial, kept only for table shape.
 */
#define STUB_TLS_SYSCALL_NR (0x0f0000 + 5)

/*
 * s390 has ONLY old_mmap (__NR_mmap = 90 -> sys_old_mmap, one arg =
 * pointer to struct mmap_arg_struct). Stage the arg struct and call
 * it with a single argument. [F-s3, P1]
 */
#define MMAP_OFFSET(o) (o)
#define STUB_MMAP_ARGSTYPE struct mmap_arg_struct

struct mmap_arg_struct {
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

#define __syscall_clobber "memory"

static __always_inline long stub_syscall0(long syscall)
{
	register long r1 __asm__("1") = syscall;
	register long r2 __asm__("2");

	__asm__ volatile ("svc 0"
		: "=d" (r2)
		: "d" (r1)
		: __syscall_clobber);

	return r2;
}

#define STUB_SYSCALL(n)							\
static __always_inline long stub_syscall##n(long syscall		\
		STUB_SYSCALL_PARM_##n)					\
{									\
	register long r1 __asm__("1") = syscall;			\
	register long r2 __asm__("2") = arg1;				\
	STUB_SYSCALL_REGS_##n						\
	__asm__ volatile ("svc 0"					\
		: "+d" (r2)						\
		: "d" (r1)						\
		: __syscall_clobber);					\
	return r2;							\
}

#define STUB_SYSCALL_PARM_1
#define STUB_SYSCALL_PARM_2 , long arg2
#define STUB_SYSCALL_PARM_3 STUB_SYSCALL_PARM_2, long arg3
#define STUB_SYSCALL_PARM_4 STUB_SYSCALL_PARM_3, long arg4
#define STUB_SYSCALL_PARM_5 STUB_SYSCALL_PARM_4, long arg5
#define STUB_SYSCALL_PARM_6 STUB_SYSCALL_PARM_5, long arg6

#define STUB_SYSCALL_REGS_0
#define STUB_SYSCALL_REGS_1
#define STUB_SYSCALL_REGS_2 register long r3 __asm__("3") = arg2
#define STUB_SYSCALL_REGS_3 STUB_SYSCALL_REGS_2;			\
	register long r4 __asm__("4") = arg3
#define STUB_SYSCALL_REGS_4 STUB_SYSCALL_REGS_3;			\
	register long r5 __asm__("5") = arg4
#define STUB_SYSCALL_REGS_5 STUB_SYSCALL_REGS_4;			\
	register long r6 __asm__("6") = arg5
#define STUB_SYSCALL_REGS_6 STUB_SYSCALL_REGS_5;			\
	register long r7 __asm__("7") = arg6

STUB_SYSCALL(1)
STUB_SYSCALL(2)
STUB_SYSCALL(3)
STUB_SYSCALL(4)
STUB_SYSCALL(5)
STUB_SYSCALL(6)

static __always_inline void trap_myself(void)
{
	/* Two illegal halfwords: s390 instruction-length code makes
	 * this a guaranteed SIGILL with ILC=4 — the tracer's trap-back
	 * notifier (no brk equivalent at EL0 without PER). */
	__asm__ (".short 0xffff\n.short 0xffff");
}

/*
 * The stub data page sits directly above the stub code page; larl
 * yields page-aligned PC, add one page.
 */
static __always_inline void *get_stub_data(void)
{
	unsigned long ret;

	asm volatile (
		"	larl	%0, 0f\n"
		"0:	aghi	%0, %1\n"
		: "=&d" (ret)
		: "i" (UM_KERN_PAGE_SIZE)
		: "cc");

	return (void *)ret;
}

#define stub_start(fn)							\
	asm volatile (							\
		"	aghi	%r15, %0\n"				\
		"	larl	%r1, %1\n"				\
		"	basr	%r14, %r1\n"				\
		:: "i" (STUB_SIZE),					\
		   "i" (&fn) : "r1", "r14", "memory")

/*
 * Nothing to restore outside the mcontext: TLS/acrs ride the frame
 * (F-s1) — unlike x86 arch_prctl and arm64 TPIDR_EL0 msr.
 */
static __always_inline void
stub_seccomp_restore_state(struct stub_data_arch *arch)
{
}

/*
 * s390 returns from signal handlers through the SA_RESTORER
 * trampoline (stub_signal_restorer, in the stub page), so the
 * rt_sigreturn svc executes from the stub page and passes the
 * seccomp filter's IP check. Nothing to do here.
 */
static __always_inline void stub_signal_return(void *frame)
{
}

/* Nothing to save: everything visible to the kernel via regsets. */
static __always_inline void stub_seccomp_save_state(struct stub_data_arch *arch)
{
}

extern void stub_segv_handler(int, siginfo_t *, void *);
extern void stub_syscall_handler(void);
extern void stub_signal_interrupt(int, siginfo_t *, void *);
extern void stub_signal_restorer(void);

#endif
