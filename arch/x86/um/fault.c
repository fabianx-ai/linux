/*
 * Copyright (C) 2002 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <arch.h>
#include <sysdep/ptrace.h>

/* These two are from asm-um/uaccess.h and linux/module.h, check them. */
struct exception_table_entry
{
	unsigned long insn;
	unsigned long fixup;
};

const struct exception_table_entry *search_exception_tables(unsigned long add);

/*
 * BPF probe-mem fixup, implemented kernel-side in bpf_fixup.c (the
 * insn decoder is not usable from this user-side object). Returns
 * nonzero when the fault was a BPF probe-mem load and was handled.
 * search_exception_tables() finds BPF entries too (it chains
 * search_bpf_extables()), so this must run before the classic path --
 * a plain ip=fixup would leave the load's destination register stale
 * instead of zeroed.
 */
#ifdef CONFIG_BPF_JIT
extern int um_bpf_fixup(unsigned long address, struct uml_pt_regs *regs);
#else
static inline int um_bpf_fixup(unsigned long address, struct uml_pt_regs *regs)
{
	return 0;
}
#endif

/* Compare this to arch/i386/mm/extable.c:fixup_exception() */
int arch_fixup(unsigned long address, struct uml_pt_regs *regs)
{
	const struct exception_table_entry *fixup;

	if (um_bpf_fixup(address, regs))
		return 1;

	fixup = search_exception_tables(address);
	if (fixup) {
		UPT_IP(regs) = fixup->fixup;
		return 1;
	}
	return 0;
}
