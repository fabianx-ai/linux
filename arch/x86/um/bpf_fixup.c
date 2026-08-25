// SPDX-License-Identifier: GPL-2.0
/*
 * BPF probe-mem fault recovery for um.
 *
 * The reused x86-64 JIT emits BPF_PROBE_MEM{,SX} loads for fault-prone
 * pointer dereferences in tracing programs, and records each load in
 * the program's exception table (x86 relative format) with the fault
 * fixup packed as: bits [7:0] = faulting insn length, bits [15:8] =
 * destination register offset (see the emission site and
 * ex_handler_bpf() on native). On native, a fault in such a load
 * zeroes the destination register and resumes after it, so a bad
 * pointer chase reads as 0 instead of killing the kernel.
 *
 * um's arch_fixup() lives in a user-side object and its local
 * exception_table_entry shadow does not match the x86 format, so the
 * handling is here instead: the BPF vector is searched first (a hit
 * is by construction an EX_TYPE_BPF entry), the destination register
 * is zeroed in the saved guest register set, and the saved ip moves
 * past the load. Under um the register offset is gp[]-array-relative
 * (see reg2pt_regs in bpf_jit_comp.c).
 */
#include <linux/extable.h>
#include <sysdep/ptrace.h>

/*
 * The x86 relative-format entry, as emitted for each probe-mem load by
 * bpf_jit_comp.c (its asm/extable.h is not includable from this um
 * object; fault.c shadows the same type the other way around). The
 * generic search resolves the relative insn address for us; only the
 * packed fixup word is read here: bits [7:0] = faulting insn length,
 * bits [15:8] = destination register gp[]-array-relative byte offset.
 */
struct exception_table_entry {
	int insn, fixup, data;
};

int um_bpf_fixup(unsigned long address, struct uml_pt_regs *regs)
{
	const struct exception_table_entry *ex;
	unsigned long insn_len, reg_off;

	ex = search_bpf_extables(address);
	if (!ex)
		return 0;

	insn_len = ex->fixup & 0xff;
	reg_off = (ex->fixup >> 8) & 0xff;
	if (insn_len == 0 || reg_off % sizeof(unsigned long) ||
	    reg_off / sizeof(unsigned long) >= MAX_REG_NR)
		return 0;

	regs->gp[reg_off / sizeof(unsigned long)] = 0;
	UPT_IP(regs) = address + insn_len;
	return 1;
}
