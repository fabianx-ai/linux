// SPDX-License-Identifier: GPL-2.0
/*
 * s390x backend ptrace.c — regsets (NT_PRSTATUS + NT_PRFPREG),
 * getreg/putreg over the UML register frame with the PSW write mask,
 * peek/poke_user, and the subarch_ptrace dispatch tail.
 */
#include <linux/sched.h>
#include <linux/elf.h>
#include <linux/regset.h>
#include <linux/uaccess.h>
/*
 * PSW user-legal bits (uapi/asm/ptrace.h) — included directly here
 * rather than via the uapi header, whose struct user_regs_struct
 * collides with the backend-owned one in asm/ptrace.h.
 */
#define UM_S390_PSW_MASK_USER	_AC(0x0000FF0180000000, UL)
#define UM_S390_PSW_MASK_PSTATE	_AC(0x0001000000000000, UL)

unsigned long getreg(struct task_struct *child, int regno)
{
	return task_pt_regs(child)->regs.gp[regno / sizeof(long)];
}

int putreg(struct task_struct *child, int regno, unsigned long value)
{
	/*
	 * Sanitize the PSW mask: keep only the user-legal bits, force
	 * problem state back. The host PTRACE_SETREGSET NT_PRSTATUS is
	 * the real backstop (it rejects illegal PSWs); this is
	 * defense-in-depth, as on arm64 for PSTATE.NZCV.
	 */
	if (regno / sizeof(long) == HOST_PSW_MASK)
		value = (value & UM_S390_PSW_MASK_USER) |
			UM_S390_PSW_MASK_PSTATE |
			(task_pt_regs(child)->regs.gp[HOST_PSW_MASK] &
			 ~UM_S390_PSW_MASK_USER);

	task_pt_regs(child)->regs.gp[regno / sizeof(long)] = value;
	return 0;
}

static int genregs_get(struct task_struct *target,
		       const struct user_regset *regset,
		       struct membuf to)
{
	int reg;

	for (reg = 0; to.left; reg++)
		membuf_store(&to, getreg(target, reg * sizeof(long)));
	return 0;
}

static int genregs_set(struct task_struct *target,
		       const struct user_regset *regset,
		       unsigned int pos, unsigned int count,
		       const void *kbuf, const void __user *ubuf)
{
	int ret = 0;

	if (kbuf) {
		const unsigned long *k = kbuf;

		while (count >= sizeof(*k) && !ret) {
			ret = putreg(target, pos, *k++);
			count -= sizeof(*k);
			pos += sizeof(*k);
		}
	} else {
		const unsigned long  __user *u = ubuf;

		while (count >= sizeof(*u) && !ret) {
			unsigned long word;

			ret = __get_user(word, u++);
			if (ret)
				break;
			ret = putreg(target, pos, word);
			count -= sizeof(*u);
			pos += sizeof(*u);
		}
	}
	return ret;
}

static int fpregs_get(struct task_struct *target,
		      const struct user_regset *regset,
		      struct membuf to)
{
	membuf_write(&to, task_pt_regs(target)->regs.fp, UM_S390_FP_SIZE);
	return 0;
}

static int fpregs_set(struct task_struct *target,
		      const struct user_regset *regset,
		      unsigned int pos, unsigned int count,
		      const void *kbuf, const void __user *ubuf)
{
	return user_regset_copyin(&pos, &count, &kbuf, &ubuf,
				  task_pt_regs(target)->regs.fp, 0,
				  UM_S390_FP_SIZE);
}

static struct user_regset uml_regsets[] __ro_after_init = {
	[REGSET_GENERAL] = {
		USER_REGSET_NOTE_TYPE(PRSTATUS),
		.n		= sizeof(struct user_regs_struct) / sizeof(long),
		.size		= sizeof(long),
		.align		= sizeof(long),
		.regset_get	= genregs_get,
		.set		= genregs_set
	},
	[REGSET_FP] = {
		USER_REGSET_NOTE_TYPE(PRFPREG),
		.n		= UM_S390_FP_SIZE / sizeof(long),
		.size		= sizeof(long),
		.align		= sizeof(long),
		.regset_get	= fpregs_get,
		.set		= fpregs_set,
	},
};

static const struct user_regset_view user_uml_view = {
	.name = "s390x", .e_machine = EM_S390,
	.regsets = uml_regsets, .n = ARRAY_SIZE(uml_regsets)
};

const struct user_regset_view *
task_user_regset_view(struct task_struct *tsk)
{
	return &user_uml_view;
}

/* read/write a word at location addr in the USER area (guest ptrace) */
int peek_user(struct task_struct *child, long addr, long data)
{
	unsigned long tmp;

	if ((addr & (sizeof(long) - 1)) || addr < 0)
		return -EIO;

	tmp = 0;  /* Default return condition */
	if (addr < MAX_REG_OFFSET)
		tmp = getreg(child, addr);
	return put_user(tmp, (unsigned long *) data);
}

int poke_user(struct task_struct *child, long addr, long data)
{
	if ((addr & (sizeof(long) - 1)) || addr < 0)
		return -EIO;

	if (addr < MAX_REG_OFFSET)
		return putreg(child, addr, data);
	return -EIO;
}

/*
 * Backend ptrace dispatch: everything not covered by the generic
 * regset path. NT_S390_SYSTEM_CALL-equivalent requests are handled
 * through the general frame here; no extra regsets in v0.
 */
long subarch_ptrace(struct task_struct *child, long request,
		    unsigned long addr, unsigned long data)
{
	return -EIO;
}
