// SPDX-License-Identifier: GPL-2.0
/*
 * F73 rung d: nested-kprobe ss_mask accounting selftest + soak.
 *
 * Registers kprobes on two static leaf functions; A's post_handler
 * calls B, producing the nested episode (REENTER) that the tracefs
 * battery cannot produce. After 1000 nested episodes, asserts:
 *   - all four handlers ran exactly 1000 times,
 *   - the per-CPU ss_mask accumulator is back to 0,
 *   - a one-shot 20 ms kernel timer fires within a 1 s window
 *     (async signals alive -- on the unfixed rule the outer frame's
 *     sigreturn keeps them masked and the timer starves).
 * Prints one F73-RUNG verdict line for the gate to grep.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kprobes.h>
#include <linux/timer.h>

extern unsigned long um_kprobe_ss_masked_read(int cpu);

static struct kprobe f73_kp_a, f73_kp_b;
static int f73_a_pre, f73_a_post, f73_b_pre, f73_b_post;
static volatile int f73_timer_fired;

static noinline void f73_target_b(void)
{
	barrier();
}

static noinline void f73_target_a(void)
{
	barrier();
}

static int f73_pre_a(struct kprobe *p, struct pt_regs *regs)
{
	f73_a_pre++;
	return 0;
}

static int f73_pre_b(struct kprobe *p, struct pt_regs *regs)
{
	f73_b_pre++;
	return 0;
}

static void f73_post_a(struct kprobe *p, struct pt_regs *regs,
		       unsigned long flags)
{
	f73_a_post++;
	f73_target_b();		/* trip the nested probe */
}

static void f73_post_b(struct kprobe *p, struct pt_regs *regs,
		       unsigned long flags)
{
	f73_b_post++;
}

static void f73_timer_fn(struct timer_list *t)
{
	f73_timer_fired = 1;
}

#define F73_SOAK 1000

static int __init f73_nested_test(void)
{
	struct timer_list t;
	unsigned long masked;
	int ret, i;

	f73_kp_a.symbol_name = "f73_target_a";
	f73_kp_a.pre_handler = f73_pre_a;
	f73_kp_a.post_handler = f73_post_a;
	f73_kp_b.symbol_name = "f73_target_b";
	f73_kp_b.pre_handler = f73_pre_b;
	f73_kp_b.post_handler = f73_post_b;

	ret = register_kprobe(&f73_kp_a);
	if (ret) {
		pr_err("F73-RUNG: register A failed %d: SKIP\n", ret);
		return 0;
	}
	ret = register_kprobe(&f73_kp_b);
	if (ret) {
		pr_err("F73-RUNG: register B failed %d: SKIP\n", ret);
		unregister_kprobe(&f73_kp_a);
		return 0;
	}

	pr_info("F73-RUNG: probes armed, starting soak\n");
	for (i = 0; i < F73_SOAK; i++)
		f73_target_a();

	masked = um_kprobe_ss_masked_read(smp_processor_id());

	timer_setup(&t, f73_timer_fn, 0);
	mod_timer(&t, jiffies + msecs_to_jiffies(20));
	msleep(1000);

	pr_info("F73-RUNG: a_pre=%d a_post=%d b_pre=%d b_post=%d masked=%lu timer=%d: %s\n",
		f73_a_pre, f73_a_post, f73_b_pre, f73_b_post, masked,
		f73_timer_fired,
		(f73_a_pre == F73_SOAK && f73_a_post == F73_SOAK &&
		 f73_b_pre == F73_SOAK && f73_b_post == F73_SOAK &&
		 masked == 0 && f73_timer_fired) ? "PASS" : "FAIL");

	timer_delete_sync(&t);
	unregister_kprobe(&f73_kp_a);
	unregister_kprobe(&f73_kp_b);
	return 0;
}
late_initcall(f73_nested_test);
