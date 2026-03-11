/**
 * @file tohost.c
 * @brief TOHOST peripheral implementation.
 *
 * Interprets the riscv-tests tohost write convention:
 *  - data == 1  -> test passed
 *  - data  > 1  -> failed at case (data >> 1)
 */

#include "rve/peripherals/tohost.h"
#include <stdio.h>

bool tohost_write(void *ctx, word addr, word data, u8 size_bits)
{
    (void)addr; (void)size_bits;
	peripheral_tohost_t *t_ctx = (peripheral_tohost_t *)ctx;

	/* riscv-tests convention: 1 = Pass, (n << 1 | 1) = Fail at case n */
	if (data == 1) {
		printf("\n[TOHOST] Test Passed\n");
		*t_ctx->exit_code = 0;
	} else {
		printf("\n[TOHOST] Test Failed at case %d\n", data >> 1);
		*t_ctx->exit_code = (int)(data >> 1);
	}

	*t_ctx->halt_flag = true;
	return true;
}

peripheral_ops_t peripheral_tohost_ops = {
	.write = tohost_write,
	.read = NULL,
	.init = NULL,
	.deinit = NULL
};
