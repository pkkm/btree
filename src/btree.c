#include "btree.h"
#include <stdlib.h>
#include <stdint.h>
#include "xassert.h"
#include "fs.h"

// The first block (address 0) of the B-tree's on-disk storage is the superblock
// (which stores metadata). The next one is the root node.

typedef uint64_t BtreePtr;
#define BTREE_NULL ((BtreePtr) -1)
// A "pointer" to a B-tree node is just the block index.

static int btree_key_cmp(BtreeKey a, BtreeKey b) {
	return (a > b) - (a < b); // Ascending order.
}

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
	int n_keys; // Not on disk; we set it after reading or before writing.
} BtreeNode;

static bool btree_node_valid(BtreeNode node) {
	if (node.n_keys < 0 || node.n_keys > BTREE_MAX_KEYS)
		return false;

	for (int i_child = 0; i_child <= node.n_keys; i_child++) {
		if (node.children[i_child] == BTREE_NULL)
			return false;
	}

	for (int i_child = node.n_keys + 1; i_child <= BTREE_MAX_KEYS; i_child++) {
		if (node.children[i_child] != BTREE_NULL)
			return false;
	}

	// Check that keys are in ascending order.
	BtreeKey prev_key = node.keys[0];
	for (int i_key = 1; i_key < node.n_keys; i_key++) {
		if (btree_key_cmp(prev_key, node.keys[i_key]) >= 0)
			return false;
		prev_key = node.keys[i_key];
	}

	return true;
}

typedef struct {
	BtreePtr next_free;
} BtreeFree; // Free block (which is always an entry in the free list).

struct Btree { // Typedef'd in the header file.
	FsFile *file;
	BtreeSuperblock superblock; // Cache.
};

void btree_read_superblock(Btree *btree) {
	fs_read(btree->file, &btree->superblock, 0, sizeof(btree->superblock));
}

void btree_write_superblock(Btree *btree) {
	fs_write(btree->file, &btree->superblock, 0, sizeof(btree->superblock));
}

BtreeFree btree_read_free(Btree *btree, BtreePtr ptr) {
	BtreeFree free;
	fs_read(btree->file, &free, ptr * BTREE_BLOCK_SIZE, sizeof(free));
	return free;
}

void btree_write_free(Btree *btree, BtreeFree free, BtreePtr ptr) {
	fs_write(btree->file, &free, ptr * BTREE_BLOCK_SIZE, sizeof(free));
}

BtreeNode btree_read_node(Btree *btree, BtreePtr ptr) {
	BtreeNode node;
	fs_read(btree->file, &node, ptr * BTREE_BLOCK_SIZE, sizeof(node));

	int i_key = 0;
	while (i_key < BTREE_MAX_KEYS && node.children[i_key + 1] != BTREE_NULL)
		i_key++;
	node.n_keys = i_key;

	xassert(2, btree_node_valid(node));
	return node;
}

void btree_write_node(Btree *btree, BtreeNode node, BtreePtr ptr) {
	xassert(2, btree_node_valid(node));
	fs_write(btree->file, &node, ptr * BTREE_BLOCK_SIZE, sizeof(node));
}

void btree_sync(Btree *btree) {
	btree_write_superblock(btree);
}

Btree *btree_new(const char *file_name) {
	Btree *btree = malloc(sizeof(*btree));
	btree->file = fs_open(file_name, true);

	fs_set_size(btree->file, BTREE_BLOCK_SIZE);
	btree->superblock.end = 1;
	btree->superblock.free_list_head = BTREE_NULL;
	btree_write_superblock(btree);

	return btree;
}

void btree_destroy(Btree *btree) {
	xassert(1, btree->file != NULL);
	btree_sync(btree);
	fs_close(btree->file);
	free(btree);
}

BtreePtr btree_alloc_node(Btree *btree) {
	BtreePtr free = btree->superblock.free_list_head;
	if (free != BTREE_NULL) {
		// If the free list is non-empty, use its first element.
		BtreePtr next_free = btree_read_free(btree, free).next_free;
		btree->superblock.free_list_head = next_free;
		return free;
	} else {
		// Otherwise, enlarge the file by 1 block.
		BtreePtr old_end = btree->superblock.end;
		btree->superblock.end++;
		fs_set_size(btree->file, btree->superblock.end * BTREE_BLOCK_SIZE);
		return old_end;
	}
}

void btree_dealloc_node(Btree *btree, BtreePtr ptr) {
	// Only adds to the free list; doesn't shrink the file.
	BtreeFree new_free;
	new_free.next_free = btree->superblock.free_list_head;
	btree_write_free(btree, new_free, ptr);
	btree->superblock.free_list_head = ptr;
}

void btree_insert(Btree *btree, BtreeKey key, BtreeValue value);
bool btree_get(Btree *btree, BtreeKey key, BtreeValue *value);

void btree_print(Btree *btree);
void btree_walk(Btree *btree, void (*callback)(BtreeKey, BtreeValue));
