#include <stdlib.h>
#include <stdint.h>
#include "xassert.h"
#include "fs.h"

typedef uint64_t BtreePtr;
enum { BTREE_NULL = 0 };
// A "pointer" to a B-tree node is just the block number.
// 0 can be used as a NULL since the first block is always the superblock.

typedef uint32_t BtreeKey;
typedef uint64_t BtreeValue;

enum {
	BTREE_BLOCK_SIZE = 512,
	BTREE_MAX_POSSIBLE_KEYS = (BTREE_BLOCK_SIZE - sizeof(BtreePtr))
		/ (sizeof(BtreeKey) + sizeof(BtreeValue) + sizeof(BtreePtr)),
	BTREE_MIN_KEYS = BTREE_MAX_POSSIBLE_KEYS / 2,
	BTREE_MAX_KEYS = BTREE_MIN_KEYS * 2
};

typedef struct {
	BtreePtr free_list_head;
	BtreePtr end; // The block after the last used one.
} BtreeSuperblock;

typedef struct {
	BtreeKey keys[BTREE_MAX_KEYS];
	BtreePtr children[BTREE_MAX_KEYS + 1];
	BtreeValue values[BTREE_MAX_KEYS];
} BtreeNode;

typedef struct {
	BtreePtr next_free;
} BtreeFree;

typedef union {
	char bytes[BTREE_BLOCK_SIZE];
	BtreeSuperblock super;
	BtreeNode node;
	BtreeFree free;
} BtreeBlock;

typedef struct {
	FsFile *file;
	BtreeBlock superblock;
} Btree;

BtreeBlock btree_read_block(Btree *btree, BtreePtr ptr) {
	BtreeBlock block;
	fs_read(btree->file, block.bytes, ptr * BTREE_BLOCK_SIZE, sizeof(block));
	return block;
}

void btree_write_block(Btree *btree, BtreeBlock block, BtreePtr ptr) {
	fs_write(btree->file, block.bytes, ptr * BTREE_BLOCK_SIZE, sizeof(block));
}

void btree_sync(Btree *btree) {
	btree_write_block(btree, btree->superblock, 0);
}

Btree *btree_new(const char *file_name) {
	Btree *btree = malloc(sizeof(*btree));
	btree->file = fs_open(file_name, true);

	fs_set_size(btree->file, BTREE_BLOCK_SIZE);
	BtreeBlock superblock;
	superblock.super.free_list_head = BTREE_NULL;
	superblock.super.end = 1;
	btree_write_block(btree, superblock, 0);

	return btree;
}

void btree_destroy(Btree *btree) {
	xassert(1, btree->file != NULL);
	btree_sync(btree);
	fs_close(btree->file);
	free(btree);
}

BtreePtr btree_alloc_node(Btree *btree) {
	BtreePtr free = btree->superblock.super.free_list_head;
	if (free != BTREE_NULL) {
		// If the free list is non-empty, use its first element.
		BtreePtr next_free = btree_read_block(btree, free).free.next_free;
		btree->superblock.super.free_list_head = next_free;
		return free;
	} else {
		// Otherwise, enlarge the file by 1 block.
		BtreePtr old_end = btree->superblock.super.end;
		btree->superblock.super.end++;
		fs_set_size(btree->file,
		            btree->superblock.super.end * BTREE_BLOCK_SIZE);
		return old_end;
	}
}

void btree_dealloc_node(Btree *btree, BtreePtr ptr) {
	// Only adds to the free list; doesn't shrink the file.
	BtreeBlock new_free;
	new_free.free.next_free = btree->superblock.super.free_list_head;
	btree_write_block(btree, new_free, ptr);
	btree->superblock.super.free_list_head = ptr;
}

void btree_insert(Btree *btree, BtreeKey key, BtreeValue value);
bool btree_get(Btree *btree, BtreeKey key, BtreeValue *value);

void btree_print(Btree *btree);
void btree_print_values(Btree *btree);

void btree_update(BtreeKey old_key, BtreeValue old_value,
                  BtreeKey new_key, BtreeValue new_value);
void btree_delete(BtreeKey key);
