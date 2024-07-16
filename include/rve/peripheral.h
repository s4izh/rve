#ifndef RVE_PERIPHERAL_H
#define RVE_PERIPHERAL_H

#include "rve/types.h"

typedef struct peripheral_t peripheral_t;

typedef struct peripheral_ops_t peripheral_ops_t;

struct peripheral_ops_t {
	// default
	u64 (*read)(void *ctx, u64 addr, u8 size);
	u64 (*write)(void *ctx, u64 addr, u64 data, u8 size);

	// size specific operations
	u8 (*read8)(void *ctx, u64 addr);
	void (*write8)(void *ctx, u64 addr, u8 data);

	u16 (*read16)(void *ctx, u64 addr);
	void (*write16)(void *ctx, u64 addr, u16 data);

	u32 (*read32)(void *ctx, u64 addr);
	void (*write32)(void *ctx, u64 addr, u32 data);

	u64 (*read64)(void *ctx, u64 addr);
	void (*write64)(void *ctx, u64 addr, u64 data);
};

struct peripheral_t {
	u64 addr_start;
	u64 addr_end;

	void *ctx;
	peripheral_ops_t *ops;

	char name[16];
};

peripheral_t *peripheral_init(u64 addr_start, u64 addr_end, void *ctx, peripheral_ops_t *ops,
			      const char *name);

void peripheral_debug(peripheral_t *p);

#endif // RVE_PERIPHERAL_H
