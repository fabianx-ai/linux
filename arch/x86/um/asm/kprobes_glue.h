/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __UM_X86_KPROBES_GLUE_H
#define __UM_X86_KPROBES_GLUE_H

/*
 * Glue for building arch/x86/kernel/kprobes/core.c as a UML subarch
 * object (D3).
 *
 * text-patching.h guards its int3_emulate_* inlines out under
 * CONFIG_UML_X86 (they use x86's named pt_regs fields); these are the
 * same operations expressed on um's PT_REGS_* accessors. The guest
 * kernel stack is ordinary in-process memory under um, so push/pop
 * are plain stores — the entry_64.S stack gap the native comments
 * describe does not exist here and is not needed.
 */

#include <linux/types.h>
#include <asm/ptrace.h>			/* PT_REGS_* accessors */
#include <asm/text-patching.h>		/* INT3/CALL_INSN_SIZE constants */
#include <uapi/asm/processor-flags.h>	/* X86_EFLAGS_* */

static __always_inline void int3_emulate_jmp(struct pt_regs *regs, unsigned long ip)
{
	PT_REGS_IP(regs) = ip;
}

static __always_inline void int3_emulate_push(struct pt_regs *regs, unsigned long val)
{
	PT_REGS_SP(regs) -= sizeof(unsigned long);
	*(unsigned long *)PT_REGS_SP(regs) = val;
}

static __always_inline unsigned long int3_emulate_pop(struct pt_regs *regs)
{
	unsigned long val = *(unsigned long *)PT_REGS_SP(regs);

	PT_REGS_SP(regs) += sizeof(unsigned long);
	return val;
}

static __always_inline void int3_emulate_call(struct pt_regs *regs, unsigned long func)
{
	int3_emulate_push(regs, PT_REGS_IP(regs) - INT3_INSN_SIZE + CALL_INSN_SIZE);
	int3_emulate_jmp(regs, func);
}

static __always_inline void int3_emulate_ret(struct pt_regs *regs)
{
	int3_emulate_jmp(regs, int3_emulate_pop(regs));
}

static __always_inline bool __emulate_cc(unsigned long flags, u8 cc)
{
	static const unsigned long cc_mask[6] = {
		[0] = X86_EFLAGS_OF,
		[1] = X86_EFLAGS_CF,
		[2] = X86_EFLAGS_ZF,
		[3] = X86_EFLAGS_CF | X86_EFLAGS_ZF,
		[4] = X86_EFLAGS_SF,
		[5] = X86_EFLAGS_PF,
	};

	bool invert = cc & 1;
	bool match;

	if (cc < 0xc) {
		match = flags & cc_mask[cc >> 1];
	} else {
		match = ((flags & X86_EFLAGS_SF) >> X86_EFLAGS_SF_BIT) ^
			((flags & X86_EFLAGS_OF) >> X86_EFLAGS_OF_BIT);
		if (cc >= 0xe)
			match = match || (flags & X86_EFLAGS_ZF);
	}

	return (match && !invert) || (!match && invert);
}

static __always_inline void int3_emulate_jcc(struct pt_regs *regs, u8 cc, unsigned long ip, unsigned long disp)
{
	if (__emulate_cc(PT_REGS_EFLAGS(regs), cc))
		ip += disp;

	int3_emulate_jmp(regs, ip);
}

#endif /* __UM_X86_KPROBES_GLUE_H */
