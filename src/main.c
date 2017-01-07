#include <time.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "btree.h"

int main() {
	srand(time(NULL));
	Btree *btree = btree_new("btree.dat");

	char *line = NULL;
	while ((line = readline("btree> "))) {
		if (line[0] != '\0')
			add_history(line);

		printf("You entered: %s\n", line);

		free(line);
	}

	btree_destroy(btree);
	return 0;
}
