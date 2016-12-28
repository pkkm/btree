#pragma once
#include <inttypes.h>

typedef uint32_t BtreeKey;
typedef uint64_t BtreeValue;
#define BTREE_KEY_PRINT PRIu32
#define BTREE_VALUE_PRINT PRIu64

typedef struct Btree Btree;

Btree *btree_new(const char *file_name);
void btree_destroy(Btree *btree);
