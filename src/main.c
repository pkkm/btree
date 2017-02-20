#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "btree.h"
#include "fs.h"
#include "recf.h"
#include "utils.h"

typedef struct {
	Btree *btree;
	Recf *recf;
} Context;

void print_key_value_record(BtreeKey key, BtreeValue value, Context *context) {
	printf("%" BTREE_KEY_PRINT " => %" BTREE_VALUE_PRINT " ==> %"
	       RECF_RECORD_PRINT "\n", key, value, recf_get(context->recf, value));
}

void list_btree_callback(BtreeKey key, BtreeValue value, void *context) {
	print_key_value_record(key, value, (Context *) context);
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

	FsStats old_btree_stats = btree_fs_stats(context->btree);
	FsStats old_recf_stats = recf_fs_stats(context->recf);

	if (n_tokens == 0)
		return;

	char *operation = tokens[0];
	char **args = &tokens[1];

	if (strcmp(operation, "get") == 0) {
		if (n_tokens != 2) {
			fprintf(stderr, "ERROR: Invalid syntax. Use: get <key>\n");
			return;
		}

		char *remaining_key;
		BtreeKey key = strtoll(args[0], &remaining_key, 10);
		if (remaining_key[0] != '\0') {
			fprintf(stderr, "ERROR: The key must be a positive integer.\n");
			return;
		}

		BtreeValue value;
		if (btree_get(context->btree, key, &value)) {
			print_key_value_record(key, value, context);
		} else {
			fprintf(stderr, "ERROR: The key %" BTREE_KEY_PRINT
			        " doesn't exist in the tree.\n", key);
		}
	} else if (strcmp(operation, "set") == 0) {
		if (n_tokens != 3) {
			fprintf(stderr, "ERROR: Invalid syntax. "
			        "Use: set <key> <record>\n");
			return;
		}

		char *remaining_key;
		BtreeKey key = strtoll(args[0], &remaining_key, 10);
		if (remaining_key[0] != '\0') {
			fprintf(stderr, "ERROR: The key must be a positive integer.\n");
			return;
		}

		char *remaining_record;
		RecfRecord record = strtoll(args[1], &remaining_record, 10);
		if (remaining_record[0] != '\0') {
			fprintf(stderr, "ERROR: The record must be a positive integer.\n");
			return;
		}

		RecfIdx idx = recf_add(context->recf, record);
		btree_set(context->btree, key, idx);
	} else if (strcmp(operation, "print-tree") == 0) {
		btree_print(context->btree, stdout);
	} else if (strcmp(operation, "list") == 0) {
		btree_walk(context->btree, &list_btree_callback, context);
	} else if (strcmp(operation, "delete") == 0) {
		fprintf(stderr, "ERROR: Not implemented.\n");
		return;
	} else {
		fprintf(stderr, "ERROR: Unknown command: %s\n", operation);
		return;
	}

	FsStats new_btree_stats = btree_fs_stats(context->btree);
	FsStats new_recf_stats = recf_fs_stats(context->recf);
	printf("Tree reads: %" PRIu64 ", writes: %" PRIu64 "; record file reads: %"
	       PRIu64 ", writes: %" PRIu64 "\n",
	       new_btree_stats.n_reads - old_btree_stats.n_reads,
	       new_btree_stats.n_writes - old_btree_stats.n_writes,
	       new_recf_stats.n_reads - old_recf_stats.n_reads,
	       new_recf_stats.n_writes - old_recf_stats.n_writes);
}

int main(int argc, char **argv) {
	srand(time(NULL));

	Context context;
	context.btree = btree_new("btree.dat");
	context.recf = recf_new("recf.dat");

	bool interactive = (argc == 1);
	if (interactive) {
		char *line = NULL;
		while ((line = readline("(btree) "))) {
			// For making a more advanced interactive interface, see
			// <http://web.mit.edu/gnu/doc/html/rlman_2.html>.

			if (line[0] != '\0')
				add_history(line);

			execute_cmd(line, &context);

			free(line);
		}
	} else {
		char *file_name = argv[1];
		FILE *file = fopen(file_name, "r");
		if (file == NULL) {
			perror("ERROR: Can't open file");
			return 1;
		}

		char *line_buffer = NULL;
		size_t line_buffer_size = 0;
		ssize_t n_read;
		while ((n_read = getline(&line_buffer, &line_buffer_size, file)) > 0) {
			printf("(btree) %s", line_buffer);
			execute_cmd(line_buffer, &context);
		}
		free(line_buffer);
	}

	recf_destroy(context.recf);
	btree_destroy(context.btree);
	return 0;
}
