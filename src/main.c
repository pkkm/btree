#include <time.h>
#include <stdlib.h>
#include "btree.h"

int main() {
	srand(time(NULL));

	Btree *btree = btree_new("btree.dat");
	btree_destroy(btree);

	return 0;
}
