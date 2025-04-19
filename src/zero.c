#define XLEN 32
#define ILEN 32

#include "rve/types.h"

struct cpu_state_s {
	u32 regs[XLEN];	// register x0 is always 0x00000000
	u32 pc;

	// flags de estado para jumps/branches

};

int main(int argc, char *argv[])
{
	return 0;
}
