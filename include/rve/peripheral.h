#ifndef RVE_PERIPHERAL_H
#define RVE_PERIPHERAL_H

#include "rve/types.h"

typedef struct peripheral_t peripheral_t;

typedef struct peripheral_ops_t peripheral_ops_t;

typedef bool (*peripheral_init_fn)(void *ctx);
typedef bool (*peripheral_deinit_fn)(void *ctx);
typedef bool (*peripheral_read_fn)(void *ctx, u64 addr, u8 size_bits, u64* result);
typedef bool (*peripheral_write_fn)(void *ctx, u64 addr, u64 data, u8 size_bits);

struct peripheral_ops_t {
	peripheral_init_fn init;
	peripheral_deinit_fn deinit;
	peripheral_read_fn read;
	peripheral_write_fn write;
    void (*tick)(void *ctx, u64 cycles_elapsed);
};

#define PERIPHERAL_NAME_MAX 32

struct peripheral_t {
	bool active;
	char name[PERIPHERAL_NAME_MAX];
	u64 addr_start;
	u64 addr_end;

	void *ctx;
	peripheral_ops_t *ops;
	void* config;
};

peripheral_t *peripheral_init(u64 addr_start, u64 addr_end, void *ctx, peripheral_ops_t *ops, const char *name);

void peripheral_debug(peripheral_t *p);

#endif // RVE_PERIPHERAL_H
