// SPDX-License-Identifier: GPL-2.0
/*
 * UML's own setjmp/longjmp for s390x.
 * Layout (must match sysdep/archsetjmp.h):
 *   r6..r13 @0..64, r14 (return address) @72, r15 (sp) @80.
 * FP registers are not touched: UML kernel threads are FP-clean
 * between switch points, same assumption as the other backends.
 */
#include <linux/linkage.h>

.text

/* int kernel_setjmp(jmp_buf *buf) — r2 = &buf[0]; returns 0 */
SYM_FUNC_START(kernel_setjmp)
	stmg	%r6,%r13,0(%r2)		/* r6..r13 @0..64 */
	stg	%r14,72(%r2)		/* return address */
	stg	%r15,80(%r2)		/* stack pointer */
	lghi	%r2,0
	br	%r14
SYM_FUNC_END(kernel_setjmp)

/*
 * void kernel_longjmp(jmp_buf *buf, int val) — r2 = &buf[0], r3 = val;
 * the restored context sees val (1 if val == 0) as setjmp's return.
 */
SYM_FUNC_START(kernel_longjmp)
	lgr	%r5,%r2			/* save buf across restores */
	lgfr	%r4,%r3			/* sign-extend int */
	lghi	%r2,1
	ltgr	%r4,%r4			/* longjmp(x,0) returns 1 */
	jne	0f
	lghi	%r4,1
0:
	lmg	%r6,%r13,0(%r5)
	lg	%r14,72(%r5)
	lg	%r15,80(%r5)
	br	%r14			/* returns %r2 = 1 or val */
SYM_FUNC_END(kernel_longjmp)
