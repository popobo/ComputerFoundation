#include <isa.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "local-include/reg.h"

extern CPU_state cpu;

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
	
	for (int i = 0; i < sizeof(regs)/sizeof(regs[0]); ++i) {
		printf("%s\t\t\t" "0x%08x" "\t\t\t%u\n", 
				regs[i], 
				cpu.gpr[i]._32,
				cpu.gpr[i]._32);
	}
}

word_t isa_reg_str2val(const char *s, bool *success) {
	if (NULL == s) {
		*success = false;
		return 0;
	}
	
	if (0 == strcmp(s, "pc"))
		return cpu.pc;

	int i = 0;

	for (; i < sizeof(regs)/sizeof(regs[0]); ++i) {
		if (0 == strcmp(s, regs[i]))
			break;
	}
	if (sizeof(regs)/sizeof(regs[0]) == i) {
		if (success != NULL)
			*success = false;
		return 0;
	}
	
	if (success != NULL)
		*success = true;
	return cpu.gpr[i]._32;
}

