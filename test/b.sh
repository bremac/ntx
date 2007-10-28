#!/bin/bash
# Script to add a note, used as an EDITOR replacement for testing.

cat > $1 <<EOF
Add deletion support to ntx via a tagged/ directory.

A secondary inverse-associative directory must be created to
hold backreferences to the tag indices which contain each
note. Ideally, these should be packed by the prefix of each
ID, in order to minimize wasted space due to block boundaries.
Given their small size and O(n) seek times, zlib compression
will likely provide benefits, possibly the ability to save
256 backreferences to a file (named with the first two hex
bytes as an identifier.)

Uncompressed format:
id [tab] tag;tag;..;tag;\n
cbab    pacman;todo;
cb2e    soulfu;todo;
cb8c    pacman;bugs;todo;
EOF
