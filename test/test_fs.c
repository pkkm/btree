// For cmocka.
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "fs.c"
#include <stdlib.h>
#include <string.h>
#include "utils.h"

FsFile *file = NULL;

static int init() {
	file = fs_open("test-file", true);
	return 0;
}

static int shutdown() {
	fs_close(file);
	return 0;
}

static void test_initial_stats() {
	assert_int_equal(file->stats.n_reads, 0);
	assert_int_equal(file->stats.n_writes, 0);
}

static const int FILE_SIZE = 2500;

static void test_read_write() {
	fs_set_size(file, FILE_SIZE);

	char data_write[FILE_SIZE];
	for (size_t i_byte = 0; i_byte < ARRAY_LEN(data_write); i_byte++)
		data_write[i_byte] = (char) rand();
	fs_write(file, data_write, 0, FILE_SIZE);

	char data_read[FILE_SIZE];
	FsOffset FIRST_PART_SIZE = 5;
	fs_read(file, data_read, 0, FIRST_PART_SIZE);
	fs_read(file, &data_read[FIRST_PART_SIZE],
	        FIRST_PART_SIZE, FILE_SIZE - FIRST_PART_SIZE);
	assert_memory_equal(data_write, data_read, FILE_SIZE);
}

static void test_final_stats() {
	assert_int_equal(file->stats.n_reads, 2);
	assert_int_equal(file->stats.n_writes, 1);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_initial_stats),
		cmocka_unit_test(test_read_write),
		cmocka_unit_test(test_final_stats),
	};

	return cmocka_run_group_tests(tests, init, shutdown);
}
