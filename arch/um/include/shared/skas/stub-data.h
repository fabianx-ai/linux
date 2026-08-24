/* SPDX-License-Identifier: GPL-2.0 */
/*

 * Copyright (C) 2015 Thomas Meyer (thomas@m3y3r.de)
 * Copyright (C) 2005 Jeff Dike (jdike@karaya.com)
 */

#ifndef __STUB_DATA_H
#define __STUB_DATA_H

#include <linux/compiler_types.h>
#include <as-layout.h>
#include <sysdep/tls.h>
#include <sysdep/stub-data.h>
#include <mm_id.h>

#define FUTEX_IN_CHILD 0
#define FUTEX_IN_KERN 1

struct stub_init_data {
	int seccomp;

	unsigned long stub_start;

	int stub_code_fd;
	unsigned long stub_code_offset;
	int stub_data_fd;
	unsigned long stub_data_offset;

	unsigned long signal_handler;
	unsigned long signal_restorer;
};

#define STUB_NEXT_SYSCALL(s) \
	((struct stub_syscall *) (((unsigned long) s) + (s)->cmd_len))

enum stub_syscall_type {
	STUB_SYSCALL_UNSET = 0,
	STUB_SYSCALL_MMAP,
	STUB_SYSCALL_MUNMAP,
};

struct stub_syscall {
	struct {
		unsigned long addr;
		unsigned long length;
		unsigned long offset;
		int fd;
		int prot;
	} mem;

	enum stub_syscall_type syscall;
};

struct stub_data {
	long err;

	int syscall_data_len;
	/* 128 leaves enough room for additional fields in the struct */
	struct stub_syscall syscall_data[(UM_KERN_PAGE_SIZE - 128) / sizeof(struct stub_syscall)] __aligned(16);

	/* data shared with signal handler (only used in seccomp mode) */
	short restart_wait;
	unsigned int futex;
	int signal;
	unsigned short si_offset;
	unsigned short mctx_offset;
	/*
	 * s390x hybrid relay: a traced stub parks in the SIGSYS
	 * signal-delivery-stop before its handler runs; the tracer
	 * (UML, via TRACEME) captures orig_gpr2 there — the true arg1,
	 * since the SIGSYS mcontext only carries r2 = -ENOSYS on s390
	 * (arch/s390/kernel/syscall.c:134) — into relay_arg1 and sets
	 * arg1_valid, then reinjects. get_stub_state() consumes and
	 * clears arg1_valid. Zero-initialized fields are ignored on
	 * x86 and arm64.
	 */
	int arg1_valid;
	unsigned long relay_arg1;

	/* seccomp architecture specific state restore */
	struct stub_data_arch arch_data;

	/* Stack for our signal handlers and for calling into .
	 * Sized per backend: arm64's signal frame exceeds one page. */
	unsigned char sigstack[UM_STUB_SIGSTACK_PAGES * UM_KERN_PAGE_SIZE] __aligned(UM_KERN_PAGE_SIZE);
};

#endif
