// Small utility macros.
#pragma once

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
