# SPDX-License-Identifier: GPL-2.0
#
# arm64 backend objects for UML. Starts with the root parameter file
# (user-offsets -> user_constants.h); the port lands the sysdep
# surfaces in dependency order (setjmp, faultinfo/mcontext, ptrace
# regsets, stub page, signal frame, vdso) and each becomes an object
# here as it is written (M3b bring-up, per DESIGN.md §6).

obj-y = subarch.o alternatives.o

# Native arm64 string library objects (string.h sets all __HAVE_ARCH_*):
# these run natively on the arm64 host, exactly like the x86 reuse.
subarch-y = ../lib/memcpy.o ../lib/memset.o ../lib/strlen.o ../lib/strcmp.o \
	../lib/strchr.o ../lib/strncmp.o ../lib/strrchr.o \
	../lib/memchr.o ../lib/memcmp.o

$(obj)/user-offsets.s: c_flags = -Wp,-MD,$(depfile) $(USER_CFLAGS)
targets += user-offsets.s

include/generated/user_constants.h: $(obj)/user-offsets.s FORCE
	$(call filechk,offsets,__USER_CONSTANT_H__)

# arm64's native generated headers (cpucap-defs.h & friends), invoked
# from the core archprepare via ARCHPREPARE_BACKEND.
PHONY += kapigen
kapigen:
	$(Q)$(MAKE) $(build)=arch/arm64/tools kapi
