#pragma once
#include <inttypes.h>
#include <stdio.h>
#include "fs.h"

// Settings.
enum { RECF_BLOCK_SIZE = 256 }; // Alignment; should be the disk's block size.
typedef uint64_t RecfRecord;
#define RECF_RECORD_PRINT PRIu64

typedef uint64_t RecfRecordIdx;

typedef struct Recf Recf;

Recf *recf_new(const char *file_name);
void recf_destroy(Recf *recf);

RecfRecordIdx recf_add(Recf *recf, RecfRecord record);
RecfRecord recf_get(Recf *recf, RecfRecordIdx idx);
void recf_delete(Recf *recf, RecfRecordIdx idx);

FsStats recf_fs_stats(Recf *recf);
