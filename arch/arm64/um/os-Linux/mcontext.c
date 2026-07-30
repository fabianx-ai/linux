// SPDX-License-Identifier: GPL-2.0
/*
 * mcontext marshal for the arm64 UML backend: uml_pt_regs <-> host
 * ucontext_t (regs[31]/sp/pc/pstate), ESR extraction from the
 * sigcontext extra-record chain, fpsimd record handling, and the
 * TPIDR_EL0 sync channel for the stub.
 */
#include <linux/errno.h>
#include <linux/string.h>
#include <sys/ucontext.h>
#include <asm/ptrace.h>
#include <sysdep/ptrace.h>
#include <sysdep/ptrace_user.h>
#include <sysdep/mcontext.h>
#include <arch.h>

#define FPSIMD_MAGIC 0x46508001
#define ESR_MAGIC 0x45535201

void get_regs_from_mc(struct uml_pt_regs *regs, mcontext_t *mc)
{
	int i;

	for (i = 0; i < 31; i++)
		regs->gp[i] = mc->regs[i];
	regs->gp[HOST_SP] = mc->sp;
	regs->gp[HOST_PC] = mc->pc;
	regs->gp[HOST_PSTATE] = mc->pstate;
}

void mc_set_rip(void *_mc, void *target)
{
	mcontext_t *mc = _mc;

	mc->pc = (unsigned long)target;
}

void get_mc_from_regs(struct uml_pt_regs *regs, mcontext_t *mc,
		      int single_stepping)
{
	int i;

	for (i = 0; i < 31; i++)
		mc->regs[i] = regs->gp[i];
	mc->sp = regs->gp[HOST_SP];
	mc->pc = regs->gp[HOST_PC];
	mc->pstate = regs->gp[HOST_PSTATE];

	/* PSTATE.SS (single-step) is bit 21 */
	if (single_stepping)
		mc->pstate |= (1UL << 21);
	else
		mc->pstate &= ~(1UL << 21);
}

void get_faultinfo_from_mc(struct faultinfo *fi, mcontext_t *mc)
{
	struct { __u32 magic; __u32 size; } *h;
	unsigned long long esr = 0;

	fi->addr = mc->fault_address;
	for (h = (void *)mc->__reserved; h->magic; h = (void *)h + h->size) {
		if (h->magic == ESR_MAGIC) {
			esr = *(__u64 *)(h + 1);
			break;
		}
	}
	fi->error_code = esr;
	fi->trap_no = (esr >> 26) & 0x3f;
}

/* Locate the fpsimd record inside the sigframe's sigcontext */
static void *get_fpstate(struct stub_data *data, mcontext_t *mcontext,
			 int *fp_size)
{
	struct { __u32 magic; __u32 size; } *h;

	for (h = (void *)mcontext->__reserved; h->magic;
	     h = (void *)h + h->size) {
		if (h->magic == FPSIMD_MAGIC) {
			if ((void *)(h + 1) + UM_FPSIMD_SIZE >
			    (void *)data->sigstack + sizeof(data->sigstack))
				return NULL;
			*fp_size = UM_FPSIMD_SIZE;
			return (void *)(h + 1);
		}
	}
	return NULL;
}

int get_stub_state(struct uml_pt_regs *regs, struct stub_data *data,
		   unsigned long *fp_size_out)
{
	mcontext_t *mcontext;
	void *fpstate_stub;
	int fp_size;

	/* mctx_offset is verified by wait_stub_done_seccomp */
	mcontext = (void *)&data->sigstack[data->mctx_offset];

	get_regs_from_mc(regs, mcontext);

	fpstate_stub = get_fpstate(data, mcontext, &fp_size);
	if (!fpstate_stub)
		return -EINVAL;

	if (fp_size_out)
		*fp_size_out = fp_size;

	if (fp_size > host_fp_size)
		return -ENOSPC;

	memcpy(&regs->fp, fpstate_stub, fp_size);

	/* TPIDR_EL0 round trip: the value lives in the arch_data channel */
	regs->gp[HOST_TLS] = data->arch_data.tls;

	return 0;
}

int set_stub_state(struct uml_pt_regs *regs, struct stub_data *data,
		   int single_stepping)
{
	mcontext_t *mcontext;
	void *fpstate_stub;
	int fp_size;

	/* mctx_offset is verified by wait_stub_done_seccomp */
	mcontext = (void *)&data->sigstack[data->mctx_offset];

	if ((unsigned long)mcontext < (unsigned long)data->sigstack ||
	    (unsigned long)mcontext >
			(unsigned long) data->sigstack +
			sizeof(data->sigstack) - sizeof(*mcontext))
		return -EINVAL;

	get_mc_from_regs(regs, mcontext, single_stepping);

	fpstate_stub = get_fpstate(data, mcontext, &fp_size);
	if (!fpstate_stub)
		return -EINVAL;

	memcpy(fpstate_stub, &regs->fp, fp_size);

	/*
	 * Sync TPIDR_EL0 through the arch channel; the stub restores it
	 * via direct msr.
	 */
	if (data->arch_data.tls != regs->gp[HOST_TLS]) {
		data->arch_data.tls = regs->gp[HOST_TLS];
		data->arch_data.sync |= STUB_SYNC_TLS;
	}

	return 0;
}
