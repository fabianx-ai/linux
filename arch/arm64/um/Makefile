# SPDX-License-Identifier: GPL-2.0
#
# arm64 backend objects for UML. Starts with the root parameter file
# (user-offsets -> user_constants.h); the port lands the sysdep
# surfaces in dependency order (setjmp, faultinfo/mcontext, ptrace
# regsets, stub page, signal frame, vdso) and each becomes an object
# here as it is written (M3b bring-up, per DESIGN.md §6).

obj-y =

$(obj)/user-offsets.s: c_flags = -Wp,-MD,$(depfile) $(USER_CFLAGS)
targets += user-offsets.s

include/generated/user_constants.h: $(obj)/user-offsets.s FORCE
	$(call filechk,offsets,__USER_CONSTANT_H__)
