// SPDX-License-Identifier: GPL-2.0
/*
 * UML's own setjmp/longjmp for s390x.
 *   r6..r13 @0..56, r14 (return address) @64, r15 (sp) @72.
 *   (The C struct in sysdep/archsetjmp.h has ten 8-byte members:
 *    eight saved GPRs then __r14 then __r15 — no padding slot at 64.
 *    The original .S stored r14@72/r15@80, one slot off, so longjmp
 *    restored r14 from the SP written by start_idle_thread and
 *    executed the stack: SIGILL ILL_ILLOPC at exactly UM_THREAD_START_SP.)
 * FP registers are not touched: UML kernel threads are FP-clean
 * between switch points, same assumption as the other backends.
 */
#include <linux/linkage.h>

.text

/* int kernel_setjmp(jmp_buf *buf) — r2 = &buf[0]; returns 0 */
SYM_FUNC_START(kernel_setjmp)
	stmg	%r6,%r13,0(%r2)		/* r6..r13 @0..56 */
	stg	%r14,64(%r2)		/* return address */
	stg	%r15,72(%r2)		/* stack pointer */
	lghi	%r2,0
	br	%r14
SYM_FUNC_END(kernel_setjmp)

/*
 * void kernel_longjmp(jmp_buf *buf, int val) — r2 = &buf[0], r3 = val;
 * the restored context sees val (1 if val == 0) as setjmp's return.
 */
SYM_FUNC_START(kernel_longjmp)
	lgr	%r5,%r2			/* buf */
	lgfr	%r2,%r3			/* return value = val */
	ltgr	%r2,%r2
	jne	0f
	lghi	%r2,1			/* longjmp(x,0) returns 1 */
0:	lmg	%r6,%r13,0(%r5)
	lg	%r14,64(%r5)
	lg	%r15,72(%r5)
	br	%r14
SYM_FUNC_END(kernel_longjmp)
