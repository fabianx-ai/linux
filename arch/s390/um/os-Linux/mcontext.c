// SPDX-License-Identifier: GPL-2.0
/*
 * mcontext marshal for the s390x UML backend: uml_pt_regs <-> host
 * ucontext_t (psw, gprs, acrs, fpregs — flat, layout-identical to
 * _sigregs per F-s1), fixed 136B FP frame handling, and the acrs TLS
 * channel. No record walk exists on s390 to mis-parse.
 */
#include <linux/errno.h>
#include <linux/string.h>
#include <sys/ucontext.h>
#include <asm/ptrace.h>
#include <sysdep/ptrace.h>
#include <sysdep/ptrace_user.h>
#include <sysdep/mcontext.h>
#include <arch.h>

void get_regs_from_mc(struct uml_pt_regs *regs, mcontext_t *mc)
{
	int i;

	regs->gp[HOST_PSW_MASK] = mc->psw.mask;
	regs->gp[HOST_PSW_ADDR] = mc->psw.addr;
	for (i = 0; i < 16; i++)
		regs->gp[HOST_GPR0 + i] = mc->gregs[i];
	/* acrs are 32-bit halves inside long slots: pack pairs of
	 * big-endian halves into the frame's HOST_ACRS long slot. */
	for (i = 0; i < 8; i++) {
		unsigned long v =
			((unsigned long)mc->aregs[2 * i] << 32) |
			mc->aregs[2 * i + 1];
		regs->gp[HOST_ACRS + i] = v;
	}
}

void mc_set_rip(void *_mc, void *target)
{
	mcontext_t *mc = _mc;

	mc->psw.addr = (unsigned long)target;
}

void get_mc_from_regs(struct uml_pt_regs *regs, mcontext_t *mc,
		      int single_stepping)
{
	int i;

	mc->psw.mask = regs->gp[HOST_PSW_MASK];
	mc->psw.addr = regs->gp[HOST_PSW_ADDR];
	for (i = 0; i < 16; i++)
		mc->gregs[i] = regs->gp[HOST_GPR0 + i];
	for (i = 0; i < 8; i++) {
		unsigned long v = regs->gp[HOST_ACRS + i];

		mc->aregs[2 * i] = (unsigned int)(v >> 32);
		mc->aregs[2 * i + 1] = (unsigned int)v;
	}

	/* PSW PER bit (single-step) is bit 63-1: 0x4000000000000000 */
	if (single_stepping)
		mc->psw.mask |= (1UL << 62);
	else
		mc->psw.mask &= ~(1UL << 62);
}

int get_stub_state(struct uml_pt_regs *regs, struct stub_data *data,
		   unsigned long *fp_size_out)
{
	mcontext_t *mcontext;

	/* mctx_offset is verified by wait_stub_done_seccomp */
	mcontext = (void *)&data->sigstack[data->mctx_offset];

	get_regs_from_mc(regs, mcontext);

	if (fp_size_out) {
		/*
		 * Size-probe call (init_seccomp sizes its allocation this
		 * way): report the frame size and return WITHOUT copying.
		 * The s390 fp frame is a compile-time constant, so unlike
		 * x86 there is no dynamic discovery here.
		 */
		*fp_size_out = UM_S390_FP_SIZE;
		return 0;
	}

	memcpy(&regs->fp, &mcontext->fpregs, UM_S390_FP_SIZE);

	/*
	 * TLS round trip from the frame: acrs[0..1] big-endian hi/lo.
	 * Verified trap-surviving via NT_PRSTATUS and mcontext (F-s1);
	 * no stub-carried channel exists or is needed.
	 */
	regs->gp[HOST_TLS] = upt_acrs_tls(regs->gp);
	regs->gp[HOST_ARG0] = regs->gp[HOST_ORIG_GPR2];
	/* int_code mirror: glibc does not carry it in mcontext_t; the
	 * syscall number reaches us through si_syscall at the caller. */

	return 0;
}

int set_stub_state(struct uml_pt_regs *regs, struct stub_data *data,
		   int single_stepping)
{
	mcontext_t *mcontext;

	/* mctx_offset is verified by wait_stub_done_seccomp */
	mcontext = (void *)&data->sigstack[data->mctx_offset];

	if ((unsigned long)mcontext < (unsigned long)data->sigstack ||
	    (unsigned long)mcontext >
			(unsigned long) data->sigstack +
			sizeof(data->sigstack) - sizeof(*mcontext))
		return -EINVAL;

	get_mc_from_regs(regs, mcontext, single_stepping);

	memcpy(&mcontext->fpregs, &regs->fp, UM_S390_FP_SIZE);

	/*
	 * Acrs ride the frame itself — nothing outside the mcontext to
	 * sync (unlike x86 fs_base/gs_base arch_prctl or arm64 TPIDR).
	 */

	return 0;
}
