#include "btree.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "xassert.h"
#include "fs.h"
#include "utils.h"

int btree_key_cmp(BtreeKey a, BtreeKey b) {
	// Ascending order, i.e. the return value will be:
	//   < 0  if a < b,
	//   == 0 if a == b,
	//   > 0  if a > b.
	return (a > b) - (a < b);
}

typedef uint64_t BtreePtr;
#define BTREE_NULL ((BtreePtr) -1)
#define BTREE_PTR_PRINT PRIu64
// A "pointer" to a B-tree node is just the block index.

enum {
	BTREE_MAX_POSSIBLE_KEYS =
		(BTREE_BLOCK_SIZE
		 - sizeof(uint8_t) - sizeof(uint16_t) - sizeof(BtreePtr))
		/ (sizeof(BtreeKey) + sizeof(BtreeValue) + sizeof(BtreePtr)),
	BTREE_MIN_KEYS = BTREE_MAX_POSSIBLE_KEYS / 2,
	BTREE_MAX_KEYS = BTREE_MIN_KEYS * 2,
	// The assignment requires BTREE_MAX_KEYS = BTREE_MIN_KEYS * 2, but we could
	// also use BTREE_MAX_KEYS = BTREE_MAX_POSSIBLE_KEYS, BTREE_MIN_KEYS =
	// BTREE_MAX_KEYS / 2 + BTREE_MAX_KEYS % 2 (division by 2, but rounded up
	// instead of down). See
	// <https://en.wikipedia.org/wiki/B-tree#Definition>.

	BTREE_MIN_CHILDREN = BTREE_MIN_KEYS + 1,
	BTREE_MAX_CHILDREN = BTREE_MAX_KEYS + 1
};

// The first block (address 0) of the B-tree's file is the superblock (which
// stores metadata).
typedef struct {
	BtreePtr root;
	BtreePtr free_list_head;
	BtreePtr end; // The block after the last used one.
} BtreeSuperblock;

typedef struct {
	BtreeKey key;
	BtreeValue value;
} BtreeItem;

static int btree_item_cmp(BtreeItem a, BtreeItem b) {
	// Convenience function.
	return btree_key_cmp(a.key, b.key);
}

typedef struct {
	bool is_leaf; // Serialized as uint8_t.
	uint16_t n_items;
	// Invariant: keys in children[i] < keys[i] < keys in children[i + 1].
	BtreeItem items[BTREE_MAX_KEYS];
	BtreePtr children[BTREE_MAX_CHILDREN];
} BtreeNode;

static bool btree_node_valid(BtreeNode node, bool is_root) {
	if (node.n_items > BTREE_MAX_KEYS)
		return false;

	if (!is_root && node.n_items < BTREE_MIN_KEYS)
		return false;

	if (!node.is_leaf) {
		for (int i_child = 0; i_child <= node.n_items; i_child++) {
			if (node.children[i_child] == BTREE_NULL)
				return false;
		}
	}

	// Check that keys are in ascending order.
	BtreeItem prev_item = node.items[0];
	for (int i_item = 1; i_item < node.n_items; i_item++) {
		if (btree_item_cmp(prev_item, node.items[i_item]) >= 0)
			return false;
		prev_item = node.items[i_item];
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

static void btree_read_superblock(Btree *btree) {
	fs_read(btree->file, &btree->superblock, 0, sizeof(btree->superblock));
}

static void btree_write_superblock(Btree *btree) {
	fs_write(btree->file, &btree->superblock, 0, sizeof(btree->superblock));
}

static BtreeFree btree_read_free(Btree *btree, BtreePtr ptr) {
	BtreeFree free;
	fs_read(btree->file, &free, ptr * BTREE_BLOCK_SIZE, sizeof(free));
	return free;
}

static void btree_write_free(Btree *btree, BtreeFree free, BtreePtr ptr) {
	fs_write(btree->file, &free, ptr * BTREE_BLOCK_SIZE, sizeof(free));
}

#define DESERIALIZE(ptr, dest, type) \
	do { \
		(dest) = *(type *) (ptr); \
		(ptr) = (char *) (ptr) + sizeof(type); \
	} while (false)

static BtreeNode btree_read_node(Btree *btree, BtreePtr ptr) {
	char block[BTREE_BLOCK_SIZE];
	fs_read(btree->file, block, ptr * BTREE_BLOCK_SIZE, sizeof(block));
	void *pos = block;

	BtreeNode node;
	DESERIALIZE(pos, node.is_leaf, uint8_t);
	DESERIALIZE(pos, node.n_items, uint16_t);
	for (int i_item = 0; i_item < BTREE_MAX_KEYS; i_item++) {
		DESERIALIZE(pos, node.items[i_item].key, BtreeKey);
		DESERIALIZE(pos, node.items[i_item].value, BtreeKey);
	}
	for (int i_child = 0; i_child < BTREE_MAX_CHILDREN; i_child++)
		DESERIALIZE(pos, node.children[i_child], BtreePtr);

	xassert(2, btree_node_valid(node, ptr == btree->superblock.root));
	return node;
}

#define SERIALIZE(ptr, src, type) \
	do { \
		*(type *) (ptr) = (src); \
		(ptr) = (char *) (ptr) + sizeof(type); \
	} while (false)

static void btree_write_node(Btree *btree, BtreeNode node, BtreePtr ptr) {
	xassert(2, btree_node_valid(node, ptr == btree->superblock.root));

	char block[BTREE_BLOCK_SIZE];
	char *end = block;

	SERIALIZE(end, node.is_leaf ? 1 : 0, uint8_t);
	SERIALIZE(end, node.n_items, uint16_t);
	for (int i_item = 0; i_item < BTREE_MAX_KEYS; i_item++) {
		SERIALIZE(end, node.items[i_item].key, BtreeKey);
		SERIALIZE(end, node.items[i_item].value, BtreeKey);
	}
	for (int i_child = 0; i_child < BTREE_MAX_CHILDREN; i_child++)
		SERIALIZE(end, node.children[i_child], BtreePtr);

	fs_write(btree->file, block, ptr * BTREE_BLOCK_SIZE, end - block);
}

static void btree_sync(Btree *btree) {
	btree_write_superblock(btree);
}

static BtreeNode btree_new_node(void) {
	BtreeNode node;
	node.n_items = 0;
	node.is_leaf = true;

	// For debugging.
	for (int i_key = 0; i_key < BTREE_MAX_KEYS; i_key++) {
		node.items[i_key].key = 0xDEADBEEF;
		node.items[i_key].value = 0xDEADBEEF;
	}

	for (int i_child = 0; i_child < BTREE_MAX_CHILDREN; i_child++)
		node.children[i_child] = BTREE_NULL;

	return node;
}

Btree *btree_new(const char *file_name) {
	Btree *btree = malloc(sizeof(*btree));

	btree->file = fs_open(file_name, true);
	fs_set_size(btree->file, BTREE_BLOCK_SIZE * 2);

	btree->superblock.root = 1;
	btree->superblock.end = 2;
	btree->superblock.free_list_head = BTREE_NULL;
	btree_write_superblock(btree);

	BtreeNode root = btree_new_node();
	btree_write_node(btree, root, btree->superblock.root);

	return btree;
}

void btree_destroy(Btree *btree) {
	xassert(1, btree->file != NULL);
	btree_sync(btree);
	fs_close(btree->file);
	free(btree);
}

static BtreePtr btree_alloc_block(Btree *btree) {
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

static void btree_dealloc_block(Btree *btree, BtreePtr ptr) {
	// Only adds to the free list; doesn't shrink the file.
	BtreeFree new_free;
	new_free.next_free = btree->superblock.free_list_head;
	btree_write_free(btree, new_free, ptr);
	btree->superblock.free_list_head = ptr;
}

static void btree_compensate(BtreeItem *separator_in_parent,
                             BtreeNode *left, BtreeNode *right,
                             BtreeItem new_item, BtreePtr new_right_child,
							 bool new_item_in_left, int i_new_item) {
	xassert(1, left->n_items < BTREE_MAX_KEYS ||
	        right->n_items < BTREE_MAX_KEYS);

	// The nodes *left and *right don't need to have a valid number of items.
	// Because of that, we can use this function to make them valid.
	xassert(1, left->n_items == 0 ||
	        btree_item_cmp(left->items[left->n_items - 1],
	                       *separator_in_parent) < 0);
	xassert(1, right->n_items == 0 ||
	        btree_item_cmp(*separator_in_parent,
	                       right->items[0]) < 0);

	xassert(1, (left->is_leaf && right->is_leaf &&
	            new_right_child == BTREE_NULL) ||
	        (!left->is_leaf && !right->is_leaf &&
	         new_right_child != BTREE_NULL));

	// Collect the items of both nodes, the item separating them and the item to
	// insert (new_item) into an array.

	int i_new_item_in_all = new_item_in_left
		? i_new_item : left->n_items + 1 + i_new_item;

	BtreeItem all_items[BTREE_MAX_KEYS * 2 + 2];
	int n_all_items = 0;

	for (int i = 0; i < left->n_items; i++) {
		if (n_all_items == i_new_item_in_all)
			all_items[n_all_items++] = new_item;
		all_items[n_all_items++] = left->items[i];
	}

	if (n_all_items == i_new_item_in_all)
		all_items[n_all_items++] = new_item;
	all_items[n_all_items++] = *separator_in_parent;

	for (int i = 0; i < right->n_items; i++) {
		if (n_all_items == i_new_item_in_all)
			all_items[n_all_items++] = new_item;
		all_items[n_all_items++] = right->items[i];
	}

	if (n_all_items == i_new_item_in_all)
		all_items[n_all_items++] = new_item;

	// Collect the children of both nodes and new_right_child into an array.

	int i_new_child = i_new_item + 1;
	int i_new_child_in_all = new_item_in_left
		? i_new_child : left->n_items + 1 + i_new_child;

	BtreePtr all_children[BTREE_MAX_CHILDREN * 2 + 1];
	int n_all_children = 0;

	for (int i = 0; i < left->n_items + 1; i++) {
		if (n_all_children == i_new_child_in_all)
			all_children[n_all_children++] = new_right_child;
		all_children[n_all_children++] = left->children[i];
	}

	for (int i = 0; i < right->n_items + 1; i++) {
		if (n_all_children == i_new_child_in_all)
			all_children[n_all_children++] = new_right_child;
		all_children[n_all_children++] = right->children[i];
	}

	if (n_all_children == i_new_child_in_all)
		all_children[n_all_children++] = new_right_child;

	// Divide the items among the left node, the place for an item in the
	// parent, and the right node.

	left->n_items = (n_all_items - 1) / 2;
	right->n_items = n_all_items - 1 - left->n_items;

	int i_next_key = 0;
	for (int i = 0; i < left->n_items; i++)
		left->items[i] = all_items[i_next_key++];
	*separator_in_parent = all_items[i_next_key++];
	for (int i = 0; i < right->n_items; i++)
		right->items[i] = all_items[i_next_key++];
	xassert(1, i_next_key == n_all_items);

	// Divide the children between the nodes.

	int i_next_child = 0;
	for (int i = 0; i < left->n_items + 1; i++)
		left->children[i] = all_children[i_next_child++];
	for (int i = 0; i < right->n_items + 1; i++)
		right->children[i] = all_children[i_next_child++];
	xassert(1, i_next_child == n_all_children);

	// Check node validity.
	xassert(2, btree_node_valid(*left, false) &&
	        btree_node_valid(*right, false));
}

typedef struct {
	BtreePtr ptr;
	BtreeNode node;
} BtreeNodeCache;

static void btree_array_insert(void *array, size_t n_elems_before_insert,
                               size_t elem_size, void *new, size_t i_new) {
	xassert(1, i_new <= n_elems_before_insert);
	memmove((char *) array + (i_new + 1) * elem_size,
			(char *) array + i_new * elem_size,
			(n_elems_before_insert - i_new) * elem_size);
	memcpy((char *) array + i_new * elem_size, new, elem_size);
}

static bool btree_set_try_compensate(Btree *btree,
                                     BtreeNode node, BtreePtr node_ptr,
                                     BtreeNode parent, BtreePtr parent_ptr,
                                     BtreeItem new_item,
                                     BtreePtr new_right_child, int i_in_node,
                                     int i_node_in_parent) {
	if (i_node_in_parent > 0) { // Has a left sibling.
		BtreePtr left_sibling_ptr = parent.children[i_node_in_parent - 1];
		BtreeNode left_sibling = btree_read_node(btree, left_sibling_ptr);

		if (left_sibling.n_items < BTREE_MAX_KEYS) {
			btree_compensate(&parent.items[i_node_in_parent - 1],
			                 &left_sibling, &node,
			                 new_item, new_right_child, false, i_in_node);
			btree_write_node(btree, parent, parent_ptr);
			btree_write_node(btree, left_sibling, left_sibling_ptr);
			btree_write_node(btree, node, node_ptr);
			return true;
		}
	}

	if (i_node_in_parent < parent.n_items) { // Has a right sibling.
		BtreePtr right_sibling_ptr = parent.children[i_node_in_parent + 1];
		BtreeNode right_sibling = btree_read_node(btree, right_sibling_ptr);

		if (right_sibling.n_items < BTREE_MAX_KEYS) {
			btree_compensate(&parent.items[i_node_in_parent],
			                 &node, &right_sibling,
			                 new_item, new_right_child, true, i_in_node);
			btree_write_node(btree, parent, parent_ptr);
			btree_write_node(btree, node, node_ptr);
			btree_write_node(btree, right_sibling, right_sibling_ptr);
			return true;
		}
	}

	return false;
}

static void btree_set_up_pass(Btree *btree, BtreeItem new_item,
                              BtreePtr new_right_child, int i_in_node,
                              BtreeNodeCache *cache, int node_depth) {
	// Insert new_item into the node stored in cache[node_depth] on position
	// i_in_node. Recurse upwards the tree (using the cache) if necessary.

	xassert(1, node_depth >= 0);

	BtreePtr node_ptr = cache[node_depth].ptr;
	BtreeNode node = cache[node_depth].node;

	xassert(1, i_in_node <= node.n_items);
	xassert(1, (node_ptr == btree->superblock.root && node_depth == 0) ||
	        (node_ptr != btree->superblock.root && node_depth > 0));
	xassert(1, (node.is_leaf && new_right_child == BTREE_NULL) ||
	        (!node.is_leaf && new_right_child != BTREE_NULL));

	// If there's free space in the node, just insert the item.

	if (node.n_items < BTREE_MAX_KEYS) {
		btree_array_insert(node.items, node.n_items, sizeof(node.items[0]),
						   &new_item, i_in_node);
		btree_array_insert(node.children, node.n_items + 1,
		                   sizeof(node.children[0]),
						   &new_right_child, i_in_node + 1);
		node.n_items++;
		btree_write_node(btree, node, node_ptr);
		return;
	}

	// The node is full. If it's not the root, try to compensate
	// (move some items to a sibling node).

	BtreePtr parent_ptr;
	int i_node_in_parent;
	if (node_ptr == btree->superblock.root) {
		parent_ptr = BTREE_NULL;
	} else {
		parent_ptr = cache[node_depth - 1].ptr;
		BtreeNode parent = cache[node_depth - 1].node;

		i_node_in_parent = 0;
		while (i_node_in_parent < BTREE_MAX_CHILDREN &&
			   parent.children[i_node_in_parent] != node_ptr)
			i_node_in_parent++;
		// Defensive programming in case the B-Tree is malformed.
		xassert(1, i_node_in_parent < BTREE_MAX_CHILDREN);

		bool compensation_successful = btree_set_try_compensate(
			btree, node, node_ptr,
			cache[node_depth - 1].node, cache[node_depth - 1].ptr,
			new_item, new_right_child, i_in_node, i_node_in_parent);
		if (compensation_successful)
			return;
	}

	// Can't compensate. We'll have to split the node (add a right sibling).

	BtreeNode new_sibling = btree_new_node();
	new_sibling.is_leaf = node.is_leaf;

	BtreeItem all_items[BTREE_MAX_KEYS + 1];
	memcpy(all_items, node.items, BTREE_MAX_KEYS * sizeof(all_items[0]));
	btree_array_insert(all_items, BTREE_MAX_KEYS, sizeof(all_items[0]),
	                   &new_item, i_in_node);

	node.n_items = BTREE_MIN_KEYS;
	memcpy(node.items, all_items, node.n_items * sizeof(node.items[0]));
	BtreeItem separator = all_items[node.n_items];
	new_sibling.n_items = ARRAY_LEN(all_items) - node.n_items - 1;
	memcpy(new_sibling.items, all_items + node.n_items + 1,
	       new_sibling.n_items * sizeof(new_sibling.items[0]));

	BtreePtr all_children[BTREE_MAX_CHILDREN + 1];
	memcpy(all_children, node.children,
	       BTREE_MAX_CHILDREN * sizeof(all_children[0]));
	btree_array_insert(all_children, BTREE_MAX_CHILDREN,
	                   sizeof(all_children[0]),
	                   &new_right_child, i_in_node + 1);

	memcpy(node.children, all_children,
	       (node.n_items + 1) * sizeof(node.children[0]));
	memcpy(new_sibling.children, all_children + node.n_items + 1,
	       (new_sibling.n_items + 1) * sizeof(new_sibling.children[0]));

	btree_write_node(btree, node, node_ptr);
	BtreePtr new_sibling_ptr = btree_alloc_block(btree);
	btree_write_node(btree, new_sibling, new_sibling_ptr);

	if (parent_ptr != BTREE_NULL) {
		btree_set_up_pass(btree, separator, new_sibling_ptr,
		                  i_node_in_parent, cache, node_depth - 1);
	} else { // We're splitting the root.
		BtreeNode new_root = btree_new_node();
		new_root.is_leaf = false;
		new_root.n_items = 1;
		new_root.items[0] = separator;
		new_root.children[0] = node_ptr;
		new_root.children[1] = new_sibling_ptr;

		btree->superblock.root = btree_alloc_block(btree);
		btree_write_node(btree, new_root, btree->superblock.root);
	}
}

static void btree_set_down_pass(Btree *btree, BtreeItem new_item,
								BtreeNodeCache *cache, BtreePtr node_ptr,
								int node_depth) {
	// Recurse down the tree to find the appropriate node for new_item and
	// insert the item there. Fill the cache while doing this.

	BtreeNode node = btree_read_node(btree, node_ptr);
	cache[node_depth].ptr = node_ptr;
	cache[node_depth].node = node;

	// Index of first key which is >= `key`, or node.n_items if there are none.
	int i_new_item = 0;
	while (i_new_item < node.n_items &&
	       btree_item_cmp(node.items[i_new_item], new_item) < 0)
		i_new_item++;

	if (i_new_item < node.n_items &&
	    btree_item_cmp(node.items[i_new_item], new_item) == 0) {
		// We found the exact key, so let's set its associated value.
		node.items[i_new_item].value = new_item.value;
		btree_write_node(btree, node, node_ptr);
		return;
	}

	if (!node.is_leaf) {
		// We know that keys[i_new_item - 1] < new_item.key < keys[i_new_item],
		// so the new_item (if it exists) will be in the i_new_item-th child's
		// subtree.
		return btree_set_down_pass(
			btree, new_item, cache, node.children[i_new_item], node_depth + 1);
	}

	btree_set_up_pass(btree, new_item, BTREE_NULL, i_new_item,
	                  cache, node_depth);
}

void btree_set(Btree *btree, BtreeKey key, BtreeValue value) {
	BtreeItem item = {key, value};
	BtreeNodeCache cache[32]; // Tree height is logarithmic in number of items,
	                          // so this should always be enough.
	btree_set_down_pass(btree, item, cache, btree->superblock.root, 0);
}

static bool btree_get_at_node(Btree *btree, BtreePtr node_ptr,
                              BtreeKey key, BtreeValue *value) {
	BtreeNode node = btree_read_node(btree, node_ptr);

	// Index of first key which is >= `key`, or node.n_items if there are none.
	int i_item = 0;
	while (i_item < node.n_items &&
	       btree_key_cmp(node.items[i_item].key, key) < 0)
		i_item++;

	if (i_item < node.n_items &&
	    btree_key_cmp(node.items[i_item].key, key) == 0) {
		// We found the key.
		if (value != NULL)
			*value = node.items[i_item].value;
		return true;
	} else if (!node.is_leaf) {
		// We know that keys[i_item - 1] < item.key < keys[i_item], so the key
		// (if it exists) will be in the i_item-th child's subtree.
		return btree_get_at_node(btree, node.children[i_item], key, value);
	} else {
		return false;
	}
}

bool btree_get(Btree *btree, BtreeKey key, BtreeValue *value) {
	return btree_get_at_node(btree, btree->superblock.root, key, value);
}

static void btree_print_at_node(Btree *btree, FILE *stream,
                                BtreePtr node_ptr, int level) {
	enum { INDENT_WIDTH = 4 };

	BtreeNode node = btree_read_node(btree, node_ptr);

	fprintf(stream, "%*sNode %" BTREE_PTR_PRINT ":\n",
	        level * INDENT_WIDTH, "", node_ptr);

	for (int i_item = 0; i_item < node.n_items; i_item++) {
		if (!node.is_leaf) {
			btree_print_at_node(btree, stream, node.children[i_item],
			                    level + 1);
		}
		printf("%*s%" BTREE_KEY_PRINT " => %" BTREE_VALUE_PRINT "\n",
		       (level + 1) * INDENT_WIDTH, "",
		       node.items[i_item].key, node.items[i_item].value);
	}
	if (!node.is_leaf && node.n_items > 0) {
		btree_print_at_node(btree, stream,
		                    node.children[node.n_items], level + 1);
	}
}

void btree_print(Btree *btree, FILE *stream) {
	btree_print_at_node(btree, stream, btree->superblock.root, 0);
}

static void btree_walk_at_node(Btree *btree, BtreePtr node_ptr,
                               void (*callback)(BtreeKey, BtreeValue, void *),
                               void *callback_context) {
	BtreeNode node = btree_read_node(btree, node_ptr);

	for (int i_item = 0; i_item < node.n_items; i_item++) {
		if (!node.is_leaf) {
			btree_walk_at_node(btree, node.children[i_item],
			                   callback, callback_context);
		}
		callback(node.items[i_item].key, node.items[i_item].value,
		         callback_context);
	}
	if (!node.is_leaf) {
		btree_walk_at_node(btree, node.children[node.n_items],
		                   callback, callback_context);
	}
}

void btree_walk(Btree *btree, void (*callback)(BtreeKey, BtreeValue, void *),
                void *callback_context) {
	btree_walk_at_node(btree, btree->superblock.root,
	                   callback, callback_context);
}

FsStats btree_fs_stats(Btree *btree) {
	return fs_stats(btree->file);
}
