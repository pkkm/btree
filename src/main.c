#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "btree.h"
#include "utils.h"

typedef struct {
	Btree *btree;
} Context;

void print_key_value(BtreeKey key, BtreeValue value) {
	printf("%" BTREE_KEY_PRINT " => %" BTREE_VALUE_PRINT "\n", key, value);
}


void list_btree_callback(BtreeKey key, BtreeValue value, void *context) {
	print_key_value(key, value);
}

void execute_cmd(char *cmd, Context *context) { // Modifies the input string.
	const char DELIMITERS[] = " \t\r\n";

	char *tokens[128];
	size_t n_tokens = 0;

	char *strtok_context;
	for (char *token = strtok_r(cmd, DELIMITERS, &strtok_context);
	     token != NULL && n_tokens < ARRAY_LEN(tokens);
	     token = strtok_r(NULL, DELIMITERS, &strtok_context))
		tokens[n_tokens++] = token;

	if (n_tokens == 0)
		return;

	if (strcmp(tokens[0], "get-raw") == 0) {
		if (n_tokens != 2) {
			fprintf(stderr, "ERROR: invalid syntax. Use: get-raw <key>\n");
			return;
		}

		char *remaining_key;
		BtreeKey key = strtoll(tokens[1], &remaining_key, 10);
		if (remaining_key[0] != '\0') {
			fprintf(stderr, "ERROR: The key must be a positive integer.\n");
			return;
		}

		BtreeValue value;
		if (btree_get(context->btree, key, &value)) {
			print_key_value(key, value);
		} else {
			fprintf(stderr, "ERROR: The key %" BTREE_KEY_PRINT
			        " doesn't exist in the tree.\n", key);
		}
	} else if (strcmp(tokens[0], "insert-raw") == 0) {
		if (n_tokens != 3) {
			fprintf(stderr, "ERROR: invalid syntax. "
			        "Use: insert-raw <key> <value>\n");
			return;
		}

		char *remaining_key;
		BtreeKey key = strtoll(tokens[1], &remaining_key, 10);
		if (remaining_key[0] != '\0') {
			fprintf(stderr, "ERROR: The key must be a positive integer.\n");
			return;
		}

		char *remaining_value;
		BtreeValue value = strtoll(tokens[2], &remaining_value, 10);
		if (remaining_value[0] != '\0') {
			fprintf(stderr, "ERROR: The value must be a positive integer.\n");
			return;
		}

		if (btree_get(context->btree, key, NULL)) {
			fprintf(stderr, "ERROR: The key already exists in the tree.\n");
			return;
		}

		btree_set(context->btree, key, value);
	} else if (strcmp(tokens[0], "print-raw") == 0) {
		btree_print(context->btree, stdout);
	} else if (strcmp(tokens[0], "list-raw") == 0) {
		btree_walk(context->btree, &list_btree_callback, NULL);
	} else {
		fprintf(stderr, "ERROR: Unknown command: %s\n", tokens[0]);
	}
}

int main() {
	srand(time(NULL));

	Context context = {NULL};
	context.btree = btree_new("btree.dat");

	char *line = NULL;
	while ((line = readline("btree> "))) {
		// For making a more advanced interactive interface, see
		// <http://web.mit.edu/gnu/doc/html/rlman_2.html>.

		if (line[0] != '\0')
			add_history(line);

		execute_cmd(line, &context);

		free(line);
	}

	btree_destroy(context.btree);
	return 0;
}
