/*
 * Copyright (C) 2026 ox-alpha (s390x UML backend)
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel.com})
 *
 * Licensed under the GPL
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <ptrace_user.h>
#include <sysdep/ptrace.h>
#include <sysdep/ptrace_user.h>
#include <linux/elf.h>

#ifndef NT_S390_SYSTEM_CALL
#define NT_S390_SYSTEM_CALL 0x307
#endif

int ptrace_getregs(long pid, unsigned long *regs_out)
{
	struct iovec iov = {
		.iov_base = regs_out,
		.iov_len = UM_FRAME_SIZE,
	};

	if (ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &iov) < 0)
		return -errno;

	/*
	 * Mirror the interruption code's syscall number into the dead
	 * HOST_SYSCALLNO slot (NT_S390_SYSTEM_CALL carries nr|0x20000 —
	 * mask to 16 bits), and the trap-time r2 into HOST_ARG0:
	 * handle_syscall's default-return write clobbers live r2 and
	 * on s390 r2 is both return value and arg1. Also assemble the
	 * big-endian acrs[0..1] TLS pair into HOST_TLS.
	 */
	{
		unsigned long sc;
		struct iovec sc_iov = { .iov_base = &sc, .iov_len = sizeof(sc) };

		if (ptrace(PTRACE_GETREGSET, pid,
			   (void *)NT_S390_SYSTEM_CALL, &sc_iov) == 0)
			regs_out[HOST_SYSCALLNO] = sc & 0xffff;
		else
			regs_out[HOST_SYSCALLNO] = -1;
	}
	regs_out[HOST_ARG0] = regs_out[HOST_ORIG_GPR2];
	regs_out[HOST_TLS] =
		(((unsigned long)(unsigned int)(regs_out[HOST_ACRS] >> 32)) << 32) |
		(unsigned int)(regs_out[HOST_ACRS]);
	return 0;
}

int ptrace_setregs(long pid, unsigned long *regs)
{
	struct iovec iov = {
		.iov_base = regs,
		.iov_len = UM_FRAME_SIZE,
	};

	if (ptrace(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &iov) < 0)
		return -errno;
	return 0;
}

const char *ptrace_reg_name(int idx)
{
#define R(n) case HOST_##n: return #n

	switch (idx) {
	R(PSW_MASK); R(PSW_ADDR);
	R(GPR0); R(GPR1);
	case HOST_GPR0 + 2: return "GPR2";
	case HOST_GPR0 + 3: return "GPR3";
	case HOST_GPR0 + 4: return "GPR4";
	case HOST_GPR0 + 5: return "GPR5";
	case HOST_GPR0 + 6: return "GPR6";
	case HOST_GPR0 + 7: return "GPR7";
	case HOST_GPR0 + 8: return "GPR8";
	case HOST_GPR0 + 9: return "GPR9";
	case HOST_GPR0 + 10: return "GPR10";
	case HOST_GPR0 + 11: return "GPR11";
	case HOST_GPR0 + 12: return "GPR12";
	case HOST_GPR0 + 13: return "GPR13";
	R(SP);
	R(ORIG_GPR2);
	R(SYSCALLNO); R(ARG0); R(TLS);
	}
	return "";
}

long sysdep_ptrace_peekuser(long pid, long off, long *val)
{
	unsigned long regs[UM_GP_SLOTS]; /* full gp frame incl. internal slots */
	int err;

	if (off == PT_SYSCALL_NR_OFFSET) {
		/* syscallno lives in its own regset (the interruption
		 * code) — reads must not alias a live GPR */
		unsigned long sc;
		struct iovec iov = { .iov_base = &sc, .iov_len = sizeof(sc) };

		if (ptrace(PTRACE_GETREGSET, pid,
			   (void *)NT_S390_SYSTEM_CALL, &iov) < 0)
			return -errno;
		*val = sc & 0xffff;
		return 0;
	}

	if (off < 0 || off >= UM_FRAME_SIZE)
		return -EIO;
	err = ptrace_getregs(pid, regs);
	if (err)
		return err;
	*val = regs[off / sizeof(long)];
	return 0;
}

long sysdep_ptrace_pokeuser(long pid, long off, long val)
{
	unsigned long regs[UM_GP_SLOTS]; /* full gp frame incl. internal slots */
	int err;

	if (off == PT_SYSCALL_NR_OFFSET) {
		/* kernel accepts a bare nr; it ORs the SVC indicator */
		unsigned long nr = (unsigned long)val & 0xffff;
		struct iovec iov = { .iov_base = &nr, .iov_len = sizeof(nr) };

		if (ptrace(PTRACE_SETREGSET, pid,
			   (void *)NT_S390_SYSTEM_CALL, &iov) < 0)
			return -errno;
		return 0;
	}

	if (off < 0 || off >= UM_FRAME_SIZE)
		return -EIO;
	err = ptrace_getregs(pid, regs);
	if (err)
		return err;
	regs[off / sizeof(long)] = val;
	return ptrace_setregs(pid, regs);
}
