#!/bin/bash
# Testing script to ensure that ntx continues functioning correctly on unix.
# Add each bug as it is found to this, to form a set of regression tests.
# They'll hardly need to be comprehensive anyway, given such a small codebase.

NTX=../src/ntx
export NTXROOT=`pwd`/"ntx_test"

# Used to manipulate ntx add/edit.
function _ntx {
  export EDITOR=`pwd`/$1
  OP=$2
  shift; shift
  $NTX $OP $@
}

function die {
  rm -r $NTXROOT
  exit -1
}

function assert {
  if [[ $1 = $2 ]]; then
    return
  else
    echo "Assertion failed: '$1' = '$2'"
    die
  fi
}

# Set up the notes through a series of clever hacks.
V=`_ntx a.sh add soulfu git todo`
Ai=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "Create a community soulfu git repository for the *nix p..."

V=`_ntx b.sh add ntx todo`
Bi=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "Add deletion support to ntx via a tagged/ directory."

V=`_ntx c.sh add pacman COW todo`
Ci=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "Complete the Copy-On-Write pacman pseudo-FS/database."

# Test listing tags. Unfortunately, can't guarantee the order.
TAGS=`$NTX tag`
assert "$TAGS" "*soulfu*"
assert "$TAGS" "*git*"
assert "$TAGS" "*todo*"
assert "$TAGS" "*ntx*"
assert "$TAGS" "*pacman*"
assert "$TAGS" "*COW*"

# Test per-tag listing. Here, we _can_ test for a definite set, and order.
assert "`$NTX tag $Ai`" "soulfu
git
todo"
assert "`$NTX tag $Bi`" "ntx
todo"
assert "`$NTX tag $Ci`" "pacman
COW
todo"

# Test per-tag retagging. Again, we test for a defined set and order.
# Re-test above tag tests.

# Test a full-index listing.
# Test several combinations and single tags.

# Test altering a note.
# Re-test above list tests.

# Test deleting a note.
# Re-test listings and tags.

# Clean up after ourselves.
rm -r $NTXROOT
