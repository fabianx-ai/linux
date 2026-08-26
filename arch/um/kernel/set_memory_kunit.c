// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit test for the ROX range registry capacity contract
 * (v3k-review R1): the registry caps at 64 concurrent ranges; the
 * 65th set_memory_rox() must fail with -ENOSPC and must not consume
 * or corrupt a slot.
 *
 * The accompanying ordering fix (slot reserved before the host
 * mprotect) is not safely observable from inside the guest -- a
 * write-probe on the failed page would panic -- so the test prints
 * the failing ("probe") page address; the host-side gate greps the
 * guest's mprotect syscalls for that address (present on the unfixed
 * tree, absent on the fixed one).
 */
#include <kunit/test.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/set_memory.h>

/* Pins MAX_ROX_RANGES in set_memory.c: a deliberate cap change must
 * update this test. */
#define ROX_KUNIT_CAP 64
/* +1 page that fails at the cap, +1 fresh page for the refill probe --
 * reusing the failed page for the refill would put a legitimate ROX
 * mprotect on it and ruin the host-side strace discriminator. */
#define ROX_KUNIT_PAGES (ROX_KUNIT_CAP + 2)

static void um_rox_capacity(struct kunit *test)
{
	unsigned long pages[ROX_KUNIT_PAGES];
	int i, nreg = 0, ret = 0;

	for (i = 0; i < ROX_KUNIT_PAGES; i++) {
		pages[i] = __get_free_page(GFP_KERNEL);
		KUNIT_ASSERT_TRUE(test, pages[i] != 0);
	}

	for (i = 0; i < ROX_KUNIT_PAGES; i++) {
		ret = set_memory_rox(pages[i], 1);
		if (ret)
			break;
		nreg++;
	}

	/* A boot-time ROX registration would make nreg < the cap; that
	 * is a changed assumption, not a registry bug -- investigate. */
	KUNIT_EXPECT_EQ_MSG(test, nreg, ROX_KUNIT_CAP,
			    "registered %d ranges before the cap (boot-time registrations?)", nreg);
	KUNIT_EXPECT_EQ(test, ret, -ENOSPC);

	if (ret) {
		/* The host-side strace gate greps for this address: the
		 * failed page must see NO mprotect on the fixed tree. */
		pr_info("um_rox_kunit: probe page %px failed with %d after %d ranges\n",
			(void *)pages[i], ret, nreg);
		/* The failed call must not consume a slot: free one, refill
		 * with the FRESH page. */
		set_memory_rw(pages[0], 1);
		KUNIT_EXPECT_EQ(test, set_memory_rox(pages[ROX_KUNIT_PAGES - 1], 1), 0);
	}

	/* Restore writability before freeing -- unconditional, because the
	 * unfixed tree leaves the failed page physically ROX and a freed
	 * ROX page would poison the allocator. */
	for (i = 0; i < ROX_KUNIT_PAGES; i++) {
		set_memory_rw(pages[i], 1);
		free_page(pages[i]);
	}
}

static struct kunit_case um_rox_test_cases[] = {
	KUNIT_CASE(um_rox_capacity),
	{}
};

static struct kunit_suite um_rox_test_suite = {
	.name = "um_rox_registry",
	.test_cases = um_rox_test_cases,
};
kunit_test_suite(um_rox_test_suite);

MODULE_DESCRIPTION("KUnit test for the UML ROX range registry");
MODULE_LICENSE("GPL");
