#!/bin/bash
# Script to add a note, used as an EDITOR replacement for testing.

cat > $1 <<EOF
Complete the Copy-On-Write pacman pseudo-FS/database.

The Copy-On-Write pseudo-FS still requires more work: In particular,
the transactional model of pacman needs to be examined to confirm
the correct workings of the pseudo-FS, and guarantee correctness.

Additionally, the hash table code should be integrated into the file
conflict resolution in a seperate patch, to prove the value of having
a hash table implementation for the corner cases.
EOF
