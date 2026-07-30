/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Register/ptrace constants for the arm64 UML backend.
 * arm64 ABI: syscall number in x8 (the kernel reloads it into
 * regs->syscallno after each ptrace stop), result in x0, pc/sp/pstate
 * as named.
 */
#include <generated/user_constants.h>

#define PT_OFFSET(r) ((r) * sizeof(long))

#define PT_SYSCALL_NR(regs) ((regs)[HOST_X8])
#define PT_SYSCALL_NR_OFFSET PT_OFFSET(HOST_X8)
#define PT_SYSCALL_RET_OFFSET PT_OFFSET(HOST_X0)

#define REGS_IP_INDEX HOST_PC
#define REGS_SP_INDEX HOST_SP

/* fpsimd register frame size (32 x uint128 + fpsr + fpcr + reserved);
 * the os-Linux side compiles against libc headers that don't carry
 * struct user_fpsimd_struct, so the size is named here for both. */
#define UM_FPSIMD_SIZE 528

/*
 * glibc may lag the kernel uapi for SYSEMU on arm64 (SYSEMU exists
 * there since Linux 5.3 — fleet finding F27); ensure definitions.
 */
#ifndef PTRACE_SYSEMU
#define PTRACE_SYSEMU 31
#endif
#ifndef PTRACE_SYSEMU_SINGLESTEP
#define PTRACE_SYSEMU_SINGLESTEP 32
#endif
