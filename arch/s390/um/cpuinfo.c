// SPDX-License-Identifier: GPL-2.0
/*
 * Host CPU feature discovery and /proc/cpuinfo display — s390x
 * backend. v0: capture the host's "facilities:" line verbatim (the
 * facility bitmap in cpuinfo_um stays unused until a consumer needs
 * it).
 */
#include <linux/seq_file.h>
#include <linux/string.h>
#include <arch.h>

static char s390_facility_line[512];

void arch_parse_cpu_flags(char *line)
{
	char *p;

	if (strncmp(line, "facilities", 10))
		return;
	p = strchr(line, ':');
	if (p) {
		p++;
		while (*p == ' ' || *p == '\t')
			p++;
		strscpy(s390_facility_line, p, sizeof(s390_facility_line));
	}
}

void arch_cpuinfo_show_extra(struct seq_file *m)
{
	seq_printf(m, "fpu\t\t: yes\n");
	seq_puts(m, "facilities    :");
	if (s390_facility_line[0])
		seq_printf(m, " %s", s390_facility_line);
	else
		seq_putc(m, '\n');
}
