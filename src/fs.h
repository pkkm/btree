// Random access file abstraction layer.
#pragma once
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint64_t FsOffset;

typedef struct {
	FILE *file;
	FsOffset size;
	unsigned int n_reads;
	unsigned int n_writes;
} FsFile;

FsFile *fs_open(const char *name, bool truncate);
void fs_close(FsFile *file);

void fs_set_size(FsFile *file, FsOffset size);

void fs_read(FsFile *file, void *dest, FsOffset offset, size_t n_bytes);
void fs_write(FsFile *file, const void *src, FsOffset offset, size_t n_bytes);
