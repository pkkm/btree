# B-tree

This is an implementation of an efficient on-disk data structure for storing key-value pairs, along with a simple command-line interface for testing it.

It uses two files. One contains the actual B-tree, which stores keys and pointers to values, and the other contains values (records). The types of keys and values are configurable in header files. So is the disk block size, which determines the size of B-tree nodes and the alignment of values. For ease of testing, keys currently are 32-bit integers, values are 64-bit integers and the block size is 256 bytes.

Despite being written as an exercise, the program is quite fast. For example, it inserts millions of numbers much faster than an one-line bash loop can print them.

## Example usage

    (btree) set 18 262144
    (btree) set 16 65536
    (btree) set 24 16777216
    (btree) set 30 1073741824
    (btree) set 10 1024
    (btree) set 28 268435456
    (btree) set 26 67108864
    (btree) set 6 64
    (btree) set 4 16
    (btree) set 8 256
    (btree) set 14 16384
    (btree) set 2 4
    (btree) set 22 4194304
    (btree) set 32 4294967296
    (btree) set 12 4096
    (btree) set 20 1048576
    (btree) print
    2 => 11 ==> 4
    4 => 8 ==> 16
    6 => 7 ==> 64
    8 => 9 ==> 256
    10 => 4 ==> 1024
    12 => 14 ==> 4096
    14 => 10 ==> 16384
    16 => 1 ==> 65536
    18 => 0 ==> 262144
    20 => 15 ==> 1048576
    22 => 12 ==> 4194304
    24 => 2 ==> 16777216
    26 => 6 ==> 67108864
    28 => 5 ==> 268435456
    30 => 3 ==> 1073741824
    32 => 13 ==> 4294967296
    (btree) print-tree
    Node 3:
        Node 1:
            2 => 11
            4 => 8
            6 => 7
            8 => 9
            10 => 4
            12 => 14
            14 => 10
        16 => 1
        Node 2:
            18 => 0
            20 => 15
            22 => 12
            24 => 2
            26 => 6
            28 => 5
            30 => 3
            32 => 13
    (btree) show-stats
    (btree) get 4
    4 => 8 ==> 16
    Tree reads: 2, writes: 0; record file reads: 0, writes: 0
    (btree) print
    2 => 11 ==> 4
    4 => 8 ==> 16
    6 => 7 ==> 64
    8 => 9 ==> 256
    10 => 4 ==> 1024
    12 => 14 ==> 4096
    14 => 10 ==> 16384
    16 => 1 ==> 65536
    18 => 0 ==> 262144
    20 => 15 ==> 1048576
    22 => 12 ==> 4194304
    24 => 2 ==> 16777216
    26 => 6 ==> 67108864
    28 => 5 ==> 268435456
    30 => 3 ==> 1073741824
    32 => 13 ==> 4294967296
    Tree reads: 3, writes: 0; record file reads: 0, writes: 0

The absence of reads and writes to the record file is not an error, it's caused by caching.

## License

    Copyright 2016, 2017 Paweł Kraśnicki.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
