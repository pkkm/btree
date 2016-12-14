// Extended <assert.h>.
#pragma once
#include <assert.h>

// Recommended assertion levels:
//   * 1 -- cheap (e.g. checking if two variables are equal).
//   * 2 -- medium (e.g. walking a linked list, or a cheap assertion executed in
//          a very tight loop).
//   * 3 -- expensive (e.g. reading a lot of data from disk).

// XASSERT_MAX_LEVEL is the maximum assertion level to execute.

#if !defined(XASSERT_MAX_LEVEL)
	#define xassert(level, expr) assert(expr)
#else
	#define xassert(level, expr) \
		do { if (level <= XASSERT_MAX_LEVEL) assert(expr); } while (0)
#endif
