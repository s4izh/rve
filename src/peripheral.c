/**
 * @file peripheral.c
 * @brief Generic peripheral helper functions.
 */

#include "rve/peripherals/peripheral.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

peripheral_t *peripheral_init(word addr_start, word addr_end, void *ctx,
			      peripheral_ops_t *ops, const char *name)
{
	peripheral_t *peripheral = malloc(sizeof(peripheral_t));
	assert(peripheral != NULL);

	peripheral->addr_start = addr_start;
	peripheral->addr_end = addr_end;
	peripheral->ctx = ctx;
	peripheral->ops = ops;

	strcpy(peripheral->name, name);

	return peripheral;
}

void peripheral_debug(peripheral_t *p)
{
	printf("%s\n", p->name);
	printf("	addr_start 0x%08X\n", p->addr_start);
	printf("	addr_end   0x%08X\n", p->addr_end);
}
