/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ELF definitions for the s390x UML backend — EM_S390 model with the
 * UML register layout (psw, gprs, acrs, orig_gpr2) and the um vDSO
 * auxv hook. Big-endian: ELF_DATA is ELFDATA2MSB (first BE UML).
 */
#ifndef __UM_S390_ELF_H
#define __UM_S390_ELF_H

#include <asm/ptrace.h>
#include <asm/user.h>
#include <skas.h>

#define CORE_DUMP_USE_REGSET

#define R_390_NONE		0
#define R_390_64		22
#define R_390_PC64		24
#define R_390_GLOB_DAT		26
#define R_390_COPY		27
#define R_390_JUMP_SLOT		28
#define R_390_RELATIVE		29
#define R_390_32		4
#define R_390_PC32		5
#define R_390_GOTPCREL		26

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) \
	((x)->e_machine == EM_S390)

#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_S390

typedef unsigned long elf_greg_t;

#define ELF_PLAT_INIT(regs, load_addr)    do { \
	int _i; \
	for (_i = 0; _i < 16; _i++) \
		(regs)->regs.gp[HOST_GPR0 + _i] = 0; \
	(regs)->regs.gp[HOST_TLS] = 0; /* exec resets the TLS pointer */ \
} while (0)

static inline void um_elf_core_copy_regs(elf_greg_t *pr_reg,
					 struct pt_regs *_regs)
{
	pr_reg[0] = (_regs)->regs.gp[HOST_PSW_MASK];
	pr_reg[1] = (_regs)->regs.gp[HOST_PSW_ADDR];
	memcpy(&pr_reg[2], &(_regs)->regs.gp[HOST_GPR0], 16 * sizeof(long));
}

#define ELF_CORE_COPY_REGS(pr_reg, _regs) um_elf_core_copy_regs(pr_reg, _regs);

#define ELF_PLATFORM_FALLBACK "s390x"

#define ARCH_HAS_SETUP_ADDITIONAL_PAGES 1
struct linux_binprm;
extern int arch_setup_additional_pages(struct linux_binprm *bprm,
	int uses_interp);

extern unsigned long um_vdso_addr;
extern unsigned long um_minsigstksz;
#define AT_SYSINFO_EHDR 33
#define ARCH_DLINFO \
	NEW_AUX_ENT(AT_SYSINFO_EHDR, um_vdso_addr); \
	NEW_AUX_ENT(AT_MINSIGSTKSZ, um_minsigstksz)
/* NEW_AUX_ENT count in ARCH_DLINFO (x86 UM leaves this 0 and relies on slack) */
#define AT_VECTOR_SIZE_ARCH 2

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_fpregs_struct elf_fpregset_t;

struct task_struct;

/*
 * AT_PAGESZ must tell the truth (F58 lesson): s390x is always 4K, so
 * PAGE_SIZE is correct by construction.
 */
#define ELF_EXEC_PAGESIZE PAGE_SIZE

#define ELF_ET_DYN_BASE ((TASK_SIZE / 3 * 2) & ~((1UL << 32) - 1))

extern long elf_aux_hwcap;
/*
 * elf_aux_hwcap mirrors the host's AT_HWCAP verbatim (shared
 * arch/um/os-Linux/elf_aux.c). No bits are stripped on s390 in v0:
 * guest and host run identically, and no facility has arm64-PAC-style
 * fork-hostility. Revisit if a facility turns out to break across
 * stub fork.
 */
#define ELF_HWCAP elf_aux_hwcap

extern char *elf_aux_platform;
#define ELF_PLATFORM (elf_aux_platform ?: ELF_PLATFORM_FALLBACK)

#endif
