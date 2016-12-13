#pragma once
#include <stdint.h>

typedef uint32_t BtreeKey;
typedef uint64_t BtreeValue;

typedef struct Btree Btree;

Btree *btree_new(const char *file_name);
void btree_destroy(Btree *btree);
