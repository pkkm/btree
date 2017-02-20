#include "recf.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "xassert.h"
#include "fs.h"
#include "utils.h"

#define RECF_NULL ((RecfIdx) -1)
#define RECF_IDX_PRINT PRIu64
// RecfIdx can be the index of a record in the file or in a block.

typedef RecfIdx RecfBlock; // Index of a block in the file.

enum {
	RECF_ITEM_SIZE = MAX(sizeof(RecfRecord), sizeof(RecfIdx)),
	RECF_MAX_RECORDS = RECF_BLOCK_SIZE / RECF_ITEM_SIZE
};

static RecfBlock recf_idx_to_block(RecfIdx idx) {
	return idx / RECF_MAX_RECORDS + 1;
}

static FsOffset recf_idx_to_disk_offset(RecfIdx idx) {
	return RECF_BLOCK_SIZE * recf_idx_to_block(idx) +
		RECF_ITEM_SIZE * (idx % RECF_MAX_RECORDS);
}

// The first block (address 0) of the file is the superblock (which stores
// metadata).
typedef struct {
	RecfIdx free_list_head;
	RecfIdx end; // Number of used records.
} RecfSuperblock;

typedef struct {
	RecfIdx next_free;
} RecfFree; // Free block (which is always an entry in the free list).

struct Recf { // Typedef'd in the header file.
	FsFile *file;
	RecfSuperblock superblock; // Cache.
};

static struct {
	bool dirty;
	RecfBlock block;
	char data[RECF_BLOCK_SIZE];
} recf_cache = {false, RECF_NULL, {0}};

void recf_cache_flush(FsFile *file) {
	if (!recf_cache.dirty)
		return;
	recf_cache.dirty = false;
	fs_write(file, recf_cache.data,
	         recf_cache.block * RECF_BLOCK_SIZE, RECF_BLOCK_SIZE);
}

void recf_cache_block(FsFile *file, RecfBlock block) {
	if (block == recf_cache.block)
		return;

	if (block != RECF_NULL)
		recf_cache_flush(file);

	fs_read(file, recf_cache.data, block * RECF_BLOCK_SIZE, RECF_BLOCK_SIZE);
	recf_cache.block = block;
}

void recf_read(FsFile *file, void *dest, FsOffset offset, size_t n_bytes) {
	RecfBlock block = offset / RECF_BLOCK_SIZE;
	recf_cache_block(file, block);

	int offset_in_block = offset - block * RECF_BLOCK_SIZE;
	memcpy(dest, recf_cache.data + offset_in_block, n_bytes);
}

void recf_write(FsFile *file, const void *src,
                FsOffset offset, size_t n_bytes) {
	RecfBlock block = offset / RECF_BLOCK_SIZE;
	recf_cache_block(file, block);

	int offset_in_block = offset - block * RECF_BLOCK_SIZE;
	memcpy(recf_cache.data + offset_in_block, src, n_bytes);
	recf_cache.dirty = true;
}

static void recf_read_superblock(Recf *recf) {
	recf_read(recf->file, &recf->superblock, 0, sizeof(recf->superblock));
}

static void recf_write_superblock(Recf *recf) {
	recf_write(recf->file, &recf->superblock, 0, sizeof(recf->superblock));
}

static RecfFree recf_read_free(Recf *recf, RecfIdx idx) {
	RecfFree free;
	recf_read(recf->file, &free, recf_idx_to_disk_offset(idx), sizeof(free));
	return free;
}

static void recf_write_free(Recf *recf, RecfFree free, RecfIdx idx) {
	recf_write(recf->file, &free, recf_idx_to_disk_offset(idx), sizeof(free));
}

static RecfRecord recf_read_record(Recf *recf, RecfIdx idx) {
	RecfRecord record;
	recf_read(recf->file, &record,
	          recf_idx_to_disk_offset(idx), sizeof(record));
	return record;
}

static void recf_write_record(Recf *recf, RecfRecord record, RecfIdx idx) {
	recf_write(recf->file, &record,
	           recf_idx_to_disk_offset(idx), sizeof(record));
}

static void recf_sync(Recf *recf) {
	recf_write_superblock(recf);
}

Recf *recf_new(const char *file_name) {
	Recf *recf = malloc(sizeof(*recf));

	recf->file = fs_open(file_name, true);
	fs_set_size(recf->file, RECF_BLOCK_SIZE);

	recf->superblock.end = 0;
	recf->superblock.free_list_head = RECF_NULL;
	recf_write_superblock(recf);

	return recf;
}

void recf_destroy(Recf *recf) {
	xassert(1, recf->file != NULL);
	recf_sync(recf);
	fs_close(recf->file);
	free(recf);
}

static RecfIdx recf_alloc_record(Recf *recf) {
	RecfIdx free_idx = recf->superblock.free_list_head;
	if (free_idx != RECF_NULL) {
		// If the free list is non-empty, use its first element.
		RecfIdx next_free = recf_read_free(recf, free_idx).next_free;
		recf->superblock.free_list_head = next_free;
		return free_idx;
	} else {
		RecfIdx old_end = recf->superblock.end;
		recf->superblock.end++;

		if (old_end == 0 ||
			recf_idx_to_block(recf->superblock.end - 1) >
			recf_idx_to_block(old_end - 1)) {
			fs_set_size(recf->file, RECF_BLOCK_SIZE *
						(recf_idx_to_block(recf->superblock.end - 1) + 1));
		}

		return old_end;
	}
}

static void recf_dealloc_record(Recf *recf, RecfIdx idx) {
	// Only adds to the free list; doesn't shrink the file.
	RecfFree new_free;
	new_free.next_free = recf->superblock.free_list_head;
	recf_write_free(recf, new_free, idx);
	recf->superblock.free_list_head = idx;
}

RecfIdx recf_add(Recf *recf, RecfRecord record) {
	RecfIdx idx = recf_alloc_record(recf);
	recf_write_record(recf, record, idx);
	return idx;
}

RecfRecord recf_get(Recf *recf, RecfIdx idx) {
	xassert(1, idx < recf->superblock.end);
	return recf_read_record(recf, idx);
}

void recf_delete(Recf *recf, RecfIdx idx) {
	xassert(1, idx < recf->superblock.end);
	recf_dealloc_record(recf, idx);
}

FsStats recf_fs_stats(Recf *recf) {
	return fs_stats(recf->file);
}
