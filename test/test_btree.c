// For cmocka.
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <time.h>
#include "btree.h"

Btree *btree = NULL;

static int init() {
	srand(time(NULL));
	btree = btree_new("test-btree.dat");
	return 0;
}

static int shutdown() {
	btree_destroy(btree);
	return 0;
}

typedef struct {
	BtreeKey key;
	BtreeValue value;
} Item;

static int item_cmp(const void *a, const void *b) {
	return btree_key_cmp(((Item *) a)->key, ((Item *) b)->key);
}

static bool item_array_has_key(Item *items, size_t n_items, BtreeKey key) {
	for (size_t i = 0; i < n_items; i++) {
		if (items[i].key == key)
			return true;
	}
	return false;
}

typedef struct {
	Item *items;
	int n_items;
	int max_n_items;
} RetrievedItems;

static void retrieve_callback(BtreeKey key, BtreeValue value, void *context) {
	RetrievedItems *retrieved = context;
	assert_true(retrieved->n_items < retrieved->max_n_items);
	retrieved->items[retrieved->n_items].key = key;
	retrieved->items[retrieved->n_items].value = value;
	retrieved->n_items++;
}

static void test_set_walk() { // The B-tree has to be empty when this is run.
	enum { MAX_N_ITEMS = 10000 };

	// Create a sorted array of items with unique keys.
	int n_items = rand() % MAX_N_ITEMS + 1;
	Item *items = malloc(n_items * sizeof(*items));
	for (int i_item = 0; i_item < n_items; i_item++) {
		BtreeKey key;
		do {
			key = rand();
		} while (item_array_has_key(items, i_item, key));
		items[i_item].key = key;
		items[i_item].value = rand();
	}
	qsort(items, n_items, sizeof(*items), item_cmp);

	// Insert the items into the tree.
	for (int i_item = 0; i_item < n_items; i_item++)
		btree_set(btree, items[i_item].key, items[i_item].value, NULL, NULL);

	// Retrieve the items from the tree.
	RetrievedItems retrieved;
	retrieved.max_n_items = n_items;
	retrieved.n_items = 0;
	retrieved.items = malloc(retrieved.max_n_items * sizeof(*retrieved.items));
	btree_walk(btree, retrieve_callback, &retrieved);

	// Verify that we got the correct items in the correct order.
	assert_int_equal(retrieved.n_items, n_items);
	for (int i_item = 0; i_item < n_items; i_item++) {
		assert_true(retrieved.items[i_item].key == items[i_item].key);
		assert_true(retrieved.items[i_item].value == items[i_item].value);
	}

	free(items);
	free(retrieved.items);
}

static void test_set_get() {
	enum { N_ITEMS = 10000 };

	for (int i_item = 0; i_item < N_ITEMS; i_item++) {
		BtreeKey key = rand();
		BtreeValue value = rand();
		btree_set(btree, key, value, NULL, NULL);

		BtreeValue retrieved_value;
		bool value_exists = btree_get(btree, key, &retrieved_value);
		assert_true(value_exists);
		assert_true(retrieved_value == value);
	}
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_set_walk),
		cmocka_unit_test(test_set_get),
	};

	return cmocka_run_group_tests(tests, init, shutdown);
}
