# REBASE-s390x-report.md
Rebase of `port/um-s390x` onto `v3/um-arm64` — 2026-08-25, RebaseAgent

## 1. Base topology and delta summary

- Old arm64 base of `port/um-s390x` (tip `a9183fe01523`): Kimi's arm64 UML tree,
  vendored as re-hashed commits; content-equivalent to branch `fix/um-arm64-v2`
  (`3adf0d372a77`), plus ~10 shared-code hardening commits carried on top.
- New base: branch `v3/um-arm64` = tag-less tip `f45935e58129` ("um: intercept
  guest syscalls on hosts without PTRACE_SYSEMU").
- merge-base(old base, v3/um-arm64) = `11028ab62899` (v7.2-rc5-76, 2026-07-29).
- Delta: `v3/um-arm64` carries **13,599** commits not in the old base — almost
  entirely upstream mainline advancement over ~26 days — plus the arm64 UML
  backend **rewritten**: Kimi's ~45-commit series was squashed/restructured into
  ~20 commits (`2a1b3bdd4b50`..`7b8aa4e1e0c4`) with additional fixes folded in
  (see §3). Patch-id comparison against v3 therefore flags even identical-intent
  base commits as "new"; the true rebase set was only the 18 s390x-specific
  commits `fc6ad3c304c0..a9183fe01523`.
- What moved in one paragraph: mainline advanced ~26 days (s390 lowcore/timex,
  ptrace, BPF JIT, PCI churn all touched files our port compiles against); the
  arm64 backend gained real-assembly stub_exe entry with a per-backend
  `STUB_EXE_START` contract (`bc86dfe9ba54`), a backend mmap hook
  (`f4c5b74db1b7`), a `stub_arch_init()` backend hook, PTRACE_SYSEMU-less host
  support (`f45935e58129`), syscall-stop hidden-register handling
  (`5006c0659ed3`), and stub signal-stack validation fixes
  (`0bcbc7c2a9b3`, `cd17aa6557bd`, `70a34f2bc0b2`). Upstream also deleted the
  generic UML `asm/timex.h` shim (`3ed403bbc967`), which broke the s390 build
  (fixed below).

## 2. Result

- Branch `port/um-s390x-v3` created in worktree
  `/home/oss-claude/kernel-work/src/rebase-s390x-wt` =
  `v3/um-arm64` + 18 rebased s390x commits + 1 adaptation commit
  `7eef28e3c518` (rebase build fixes). Not pushed.
- s390x cross-build: **SUCCESS**.
  `PATH=/tmp/xgcc/usr/bin:/tmp/s390as:/usr/bin:/bin make O=.build-s390x-v3
  ARCH=um SUBARCH=s390 CROSS_COMPILE=s390x-linux-gnu- -j8` links `linux`
  (ELF 64-bit MSB IBM S/390, BuildID 30811092bd65be01b8014a594445afe52067f4fb).
- No boot testing performed (box off-limits); see §5 for runtime caveats.

## 3. Conflict inventory (file / s390x intent / v3 change / resolution)

All conflicts occurred while replaying the 18-commit s390x series; new hashes
on `port/um-s390x-v3` given in parentheses.

1. `arch/arm64/um/shared/sysdep/stub.h` — in `8273876f77fb` → `28dadc9cb6ff`
   (s390x stub_segv + STUB_MMAP_CALL hook).
   Intent: comment documenting the backend mmap invocation hook.
   v3 change: same comment, different wording.
   Resolution: took v3's wording. Cosmetic only.
2. `arch/um/kernel/skas/syscall.c` — in `998eebf59588` → `512f09c0fe44`
   (first-boot blocker batch), again in `8bc5e272e866` → `566d218e4e15`
   (shell milestone), again in `419ca907bb24` → `72c07a1a087e` (TEMP strip).
   Intent: unconditional `PT_REGS_SET_SYSCALL_RETURN(regs, -ENOSYS)` seeding
   plus TEMP debug prints (later stripped by design).
   v3 change: `2e2a827da021` wrapped the seeding in
   `if (UM_SEED_ENOSYS_BEFORE_TRACE(r))` (default 1) to mirror native arm64.
   Resolution: took v3's guarded form in all three conflicts; the s390x TEMP
   prints were never introduced, making the later strip commit's hunk there an
   empty no-op resolved to v3's side. Semantics for s390x unchanged
   (default-on seeding preserved via the macro default).
3. `arch/um/os-Linux/skas/process.c`, futex ETIMEDOUT branch — in
   `998eebf59588` → `512f09c0fe44`.
   Intent: comment wording + TEMP `os_info` wait-timeout print.
   v3 change: comment wording only.
   Resolution: took v3's side (TEMP print dropped; it was stripped later anyway).
4. `arch/um/kernel/skas/Makefile` — in `d77dea1f6edc` → `3b1f53052da6`
   (hybrid relay v1).
   Intent: pass `-DCONFIG_UML_S390 $(USER_CFLAGS_S390_ISYSTEM)` explicitly to
   stub TUs (no autoconf in stub; multiarch libc headers must win on cross
   builds) — commit `6c9fba73234b` → `f1c59fe65e68` adds the isystem variable.
   v3 change: filter `-mgeneral-regs-only` out of USER_CFLAGS (arm64
   call-closedness fix).
   Resolution: kept BOTH blocks — they are independent and additive.
5. `arch/um/os-Linux/skas/process.c`, futex bounded-wait backstop — in
   `d77dea1f6edc` → `3b1f53052da6`.
   Intent: on timeout, if `data->tracer_ready` is armed, reap the relay
   breakpoint trap (`s390_reap_stub_trap()`) before continuing the wait — core
   tracer-lane relay logic.
   v3 change: comment-only edit at the same spot.
   Resolution: kept the s390x code (relay intent); v3's comment is subsumed.
6. `arch/um/os-Linux/skas/process.c`, `wait_stub_done_seccomp()` — in
   `b251b3192e89` → `728b35bdaf62` (relay v4, orig_gpr2 capture at SIGSYS
   delivery-stop).
   Intent: add `int seen_signal = -1;` local (later auto-merged uses at
   lines 412–438 depend on it).
   v3 change: none at that spot (empty HEAD side).
   Resolution: took the s390x side.
7. `arch/um/kernel/skas/stub_exe.c` — in `cf1df1098496` → `b37da3303689`
   (support-doc §6 deferred list).
   Intent: replace naked-C entry with real file-scope s390 asm `_start`
   (GCC ignores naked on s390) calling `real_init`; drop the old `stub_start`
   macro from arch/s390/um/shared/sysdep/stub.h.
   v3 change: `bc86dfe9ba54` moved the `_start` prologue into shared
   stub_exe.c and made each backend supply only the entry BODY via
   `STUB_EXE_START` in sysdep/stub.h; `real_init` became
   `static __attribute__((used))`.
   Resolution: adopted v3's structure wholesale — kept v3's declaration and
   generic `__asm__("_start: … STUB_EXE_START …")` block, deleted the s390
   inline asm, and added the s390 body macro (aghi %r15,-STUB_SIZE; brasl
   real_init; j .) to arch/s390/um/shared/sysdep/stub.h.

Post-rebase build breaks fixed in `7eef28e3c518` (not git conflicts):

8. `arch/s390/um/asm/timex.h` (new shadow header). Upstream `3ed403bbc967`
   removed the empty UML timex shim, so `<linux/timex.h>` fell through to
   native arch/s390/include/asm/timex.h whose `#include <asm/lowcore.h>`
   needs `psw_t` — undefined in UML TUs where `asm/ptrace.h` resolves to the
   backend shadow. Restored the pre-removal behavior (CLOCK_TICK_RATE +
   asm-generic/timex.h) as a backend shadow, matching this port's established
   shadow-header pattern.
9. `arch/s390/um/shared/sysdep/stub.h`: added the new mandatory
   `stub_arch_init()` no-op hook (v3's stub_exe calls it unconditionally;
   x86 has the same no-op) and `#include <linux/stringify.h>` like sibling
   backends (needed by STUB_EXE_START).

## 4. Bug-fix extraction: v3 changes relevant to s390x

Scope: commits in `fix/um-arm64-v2..v3/um-arm64` touching arch/um shared code
or backend files, assessed for the s390x port. Verdicts:
already-correct-in-s390x / inherited-by-rebase / needs-manual-port-to-s390x /
not-applicable.

### Shared arch/um code

- `0bcbc7c2a9b3` "do not accept a signal frame that does not fit the stub data
  area" — process.c:get_stub_state() now rejects mctx_offset when
  sizeof(sigstack) < sizeof(mcontext_t) (arm64 mcontext ≈4.4 KB vs 4 KB page
  turned a broken host into OOB reads elsewhere). Verdict:
  **inherited-by-rebase**, and directly useful to the open BUG B investigation
  (stub_data corruption): a stale/garbage si_offset/mctx_offset now fails
  loudly instead of misreading.
- `cd17aa6557bd` "give the seccomp probe a legal signal stack" — start_up.c
  boot probe gets an altstack-sized signal stack and reports failures. Verdict:
  **inherited-by-rebase** (s390x defaults to seccomp userspace and runs this
  probe path).
- `70a34f2bc0b2` "read AT_MINSIGSTKSZ and report the stub's signal stack
  against it" — elf_aux.c/start_up.c. Verdict: **inherited-by-rebase**;
  complements the s390x-side AT_MINSIGSTKSZ advertisement from `cf1df1098496`.
- `f45935e58129` "intercept guest syscalls on hosts without PTRACE_SYSEMU" —
  process.c/start_up.c (+arm64 sysdep): tracer-lane fallback that cancels
  in-flight syscalls by number rewrite when the host lacks PTRACE_SYSEMU.
  Verdict: **inherited-by-rebase**, but flagged **needs-runtime-verification**
  on s390x: the s390 hybrid relay coexists with these lanes behind
  CONFIG_UML_S390 gates and the tree builds, but boot behavior was NOT
  verified (box off-limits). Note the s390 fact "SYSEMU does not skip
  syscalls" makes the seccomp relay primary regardless.
- `5006c0659ed3` "step off the syscall stop before touching the register set"
  — x7-style hidden-register handling around syscall stops, gated on
  UM_SYSCALL_STOP_HIDES_REG. s390 defines no such register (delivery-stop PSW
  already sits at svc+2; gpr contents fully visible), so the pseudo-step logic
  compiles out. Verdict: **not-applicable** (correctly inert for s390x).
- `2e2a827da021` "-ENOSYS default return the way native arm64 does" — shared
  syscall.c hook UM_SEED_ENOSYS_BEFORE_TRACE, default 1. Verdict:
  **inherited-by-rebase**; s390x keeps the seeding it requires (r2=-ENOSYS).
- `3445270cf15b` "size gp register buffers from UM_GP_SLOTS" — verified present
  post-rebase (exec_regs[UM_GP_SLOTS], syscall_regs[UM_GP_SLOTS] in mem.c).
  Verdict: **inherited-by-rebase**; s390x depends on it.
- `b83d06491ac7` "parameterize the thread start stack pointer" —
  UM_THREAD_START_SP hook; s390x defines its own in
  arch/s390/um/shared/sysdep/ptrace.h (UM_THREAD_START_SP+112 backchain,
  box-proven). Verdict: **already-correct-in-s390x**.
- `875f8b0e49b9` "bound the seccomp stub wait and probe dead stubs" — present
  in the old base already (`76dbaf6ea8e3` equivalent); bounded futex wait +
  stub_is_dead probe survive in the rebased tree. Verdict:
  **already-correct-in-s390x**.
- `0657d8bbfd4e` "refuse to boot when host page size exceeds guest's",
  `8b68245b9e93` "4-level page-table geometry from PAGE_SHIFT" — mm/config
  guards. Verdict: **inherited-by-rebase** (protects 4K-guest s390x on
  large-page hosts; geometry now PAGE_SHIFT-correct).
- `3569c540c579` chan EOF console fix, `e3b1d69473b7` uml_dir NULL deref,
  `1a6807561ee9` fault address in kernel-mode segv panics (pairs well with the
  s390x si_code write-classification commit `a9183fe01523`),
  `63354f75a9da` uml.lds.S section naming, `4c96fbe3c143` registers.h decls,
  `d90daf84b9a3` __sprintf_chk prototype. Verdict: all
  **inherited-by-rebase**; hygiene/robustness, none require s390x action.
- mm/mmap teardown races: **no new v3 commits** in this range touch
  skas/mmu.c allocation/free paths beyond the above (get_order() stub_data
  sizing from the old base is retained; verified in rebased mmu.c).

### Backend contracts reworked by v3 (port work done during this rebase)

- `bc86dfe9ba54` real-assembly stub_exe entry + `STUB_EXE_START`,
  `f4c5b74db1b7` backend STUB_MMAP_CALL hook, `stub_arch_init()` hook.
  Verdict: were **needs-manual-port-to-s390x** — done in this rebase
  (conflict #7 and fix commit `7eef28e3c518`).

### Arm64-specific fixes analyzed for s390x lessons

- `a97441b7ce9f` mask PSTATE writes from sigreturn frame — arm64-only; s390x
  controls PSW explicitly in its own frame builder (delivery-stop PSW=svc+2
  rule). Verdict: **not-applicable** (concept: keep guest-invisible state out
  of frame restore — s390x already conforms).
- `a5c4660ec87a` pin vDSO trampoline ABI offsets to signal.c layout —
  arm64-only files; symptom (garbage unwinds out of handlers) would apply to
  the s390x vDSO sigreturn trampoline too if arch/s390/um/signal.c frame
  layout moves. Verdict: **needs-manual-port-to-s390x**, low priority
  (debug-only symptom; add offset asserts to arch/s390/um/vdso/sigreturn.S).
- `3bab1854b946` TPIDR_EL0 through ptrace channel — s390x has no stub-carried
  TLS (acrs are ptrace-visible on both routes). Verdict: **not-applicable**.
- `757d07fd9efd` PAC keys NOP-host disable, `6bca2450381a` R_AARCH64_NONE
  relocation value, `8d170a9762d2` loadable modules — arm64 machinery.
  Verdict: **not-applicable**.

## 5. Not finished / caveats

- No runtime verification possible (boot box off-limits). In particular the
  interaction of the s390x hybrid relay with the new PTRACE_SYSEMU-fallback
  tracer lanes (`f45935e58129`) and with `have_ptrace_sysemu` probing should
  be re-booted on the box before trusting the debugger lane under nosysemu
  hosts. The known-open bugs A (pipe-read EFAULT), B (stub_data corruption),
  C (clone hang) remain as documented in s390-state.md; nothing in this rebase
  addresses them beyond inheriting the get_stub_state bound check.
- Commit `6b4bd1542a46` (docs: S390x-support review copy) rebased cleanly and
  still references the pre-rebase hashes.
- The adaptation commit `7eef28e3c518` touches only arch/s390/um; diff of
  arch/s390 between old and new branches shows otherwise just upstream
  mainline drift (native s390 drivers), as expected.
