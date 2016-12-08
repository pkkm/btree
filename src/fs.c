#include "fs.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "xassert.h"

FsFile *fs_open(const char *name, bool truncate) {
	FsFile *file = malloc(sizeof(*file));
	xassert(1, file != NULL);

	file->n_reads = 0;
	file->n_writes = 0;

	file->file = fopen(name, truncate ? "w+b" : "a+b");
	xassert(1, file->file != NULL);

	struct stat file_stat;
	int fstat_result = fstat(fileno(file->file), &file_stat);
	xassert(1, fstat_result != -1);
	file->size = file_stat.st_size;

	return file;
}

void fs_close(FsFile *file) {
	xassert(1, file != NULL);
	fclose(file->file);
	free(file);
}

void fs_set_size(FsFile *file, FsOffset size) {
	xassert(1, file != NULL);

	int ftruncate_result = ftruncate(fileno(file->file), size);
	xassert(1, ftruncate_result != -1);
	file->size = size;
}

void fs_read(FsFile *file, void *dest, FsOffset offset, size_t n_bytes) {
	xassert(1, file != NULL);
	xassert(1, offset < file->size);

	file->n_reads++;

	ssize_t pread_result = pread(fileno(file->file), dest, n_bytes, offset);
	xassert(1, pread_result == n_bytes);
}

void fs_write(FsFile *file, const void *src, FsOffset offset, size_t n_bytes) {
	xassert(1, file != NULL);
	xassert(1, offset < file->size);

	file->n_writes++;

	ssize_t pwrite_result = pwrite(fileno(file->file), src, n_bytes, offset);
	xassert(1, pwrite_result == n_bytes);
}
