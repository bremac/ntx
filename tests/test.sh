#!/bin/bash
# Testing script to ensure that ntx continues functioning correctly on unix.
# Add each bug as it is found to this, to form a set of regression tests.
# They'll hardly need to be comprehensive anyway, given such a small codebase.

NTX=../ntx
TAB="	"
EDIT=editor.sh
export NTXROOT=`pwd`/"ntx_test"

if [ -d $NTXROOT ]; then
  rm -r $NTXROOT
fi

####         The Test Notes         ####
A="Create a community soulfu git repository for the *nix port.

Unfortunately, the current soulfu 'development' model needs work:
Changes made are not committed to CVS, and developer time is wasted
recreating works already created and documented by others. The
gains from CVS are obviously paltry compared to those offered by
a more distributed model, given the amount of work which is required,
so a mob branch will be opened to all in order to prompt the fastest
progress possible.

Contingent on this is liberating the engine source under the GPL, so
that works can be _legally_ distributed, instead of sneaking around
and hoping to find a loophole."

B="Add deletion support to ntx via a tagged/ directory.

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
cb8c    pacman;bugs;todo;"

C="Complete the Copy-On-Write pacman pseudo-FS/database.

The Copy-On-Write pseudo-FS still requires more work: In particular,
the transactional model of pacman needs to be examined to confirm
the correct workings of the pseudo-FS, and guarantee correctness.

Additionally, the hash table code should be integrated into the file
conflict resolution in a seperate patch, to prove the value of having
a hash table implementation for the corner cases."


# Used to manipulate ntx add/edit.
function _ntx {
  export EDITOR=`pwd`/$1
  OP=$2
  shift; shift
  $NTX $OP $@
}

# Used to generate a script for EDITOR.
function ed_write {
  echo "#!/bin/bash" > $EDIT
  echo 'cat > $1 <<EOF' >> $EDIT
  echo "$1" >> $EDIT
  echo 'EOF' >> $EDIT
  chmod +x $EDIT
}

function die {
  echo "The directory $NTXROOT has been preserved for inspection."
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
ed_write "$A"
V=`_ntx $EDIT add soulfu git todo`
Av="Create a community soulfu git repository for the *nix p..."
Ai=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "$Av"

ed_write "$B"
V=`_ntx $EDIT add ntx todo`
Bv="Add deletion support to ntx via a tagged/ directory."
Bi=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "$Bv"

ed_write "$C"
V=`_ntx $EDIT add pacman COW todo`
Cv="Complete the Copy-On-Write pacman pseudo-FS/database."
Ci=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "$Cv"

# Test listing notes - FIFO ordering is assumed.
assert "`$NTX list`" "$Ai$TAB$Av
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list todo`" "$Ai$TAB$Av
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list todo git`" "$Ai$TAB$Av"
assert "`$NTX list pacman ntx`" ""

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
$NTX tag $Ai soulfu git todo unix
assert "`$NTX tag $Ai`" "soulfu
git
todo
unix"
$NTX tag $Ci pacman todo COW unix
assert "`$NTX tag $Ci`" "pacman
todo
COW
unix"

# Re-test above tag tests.
TAGS=`$NTX tag`
assert "$TAGS" "*soulfu*"
assert "$TAGS" "*git*"
assert "$TAGS" "*todo*"
assert "$TAGS" "*ntx*"
assert "$TAGS" "*pacman*"
assert "$TAGS" "*COW*"
assert "$TAGS" "*unix*"

# Re-test listing notes for unix.
assert "`$NTX list unix`" "$Ai$TAB$Av
$Ci$TAB$Cv"

# Test removing a tag and altering one.
$NTX tag $Bi ntx done
$NTX tag $Ci pacman COW unix

# Re-test above tags tests.
TAGS=`$NTX tag`
assert "$TAGS" "*soulfu*"
assert "$TAGS" "*git*"
assert "$TAGS" "*todo*"
assert "$TAGS" "*ntx*"
assert "$TAGS" "*pacman*"
assert "$TAGS" "*COW*"
assert "$TAGS" "*done*"
assert "$TAGS" "*unix*"

assert "`$NTX list todo`" "$Ai$TAB$Av"
assert "`$NTX list done`" "$Bi$TAB$Bv"
assert "`$NTX list unix`" "$Ai$TAB$Av
$Ci$TAB$Cv"

# Reset the tags back to normal for later tests.
$NTX tag $Bi ntx todo
$NTX tag $Ci pacman todo COW unix

# Test altering a note - Edit A to be equal to b.
ed_write "$B"
V=`_ntx $EDIT edit $Ai`
assert "`echo $V | cut -b 6-`" "$Bv"

# Re-test to ensure the index and tags have changed.
assert "`$NTX list`" "$Ai$TAB$Bv
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list todo git`" "$Ai$TAB$Bv"

# Test resetting A to itself via stdin.
V=`echo $A | $NTX edit $Ai`
assert "`echo $V | cut -b 6-`" "$Av"

# Re-test to ensure the index and tags have changed.
assert "`$NTX list`" "$Ai$TAB$Av
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list todo git`" "$Ai$TAB$Av"

# Test deleting a note - Delete A from the set.
$NTX rm $Ai

# Re-test listings and tags.
assert "`$NTX list`" "$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list todo`" "$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list git`" ""

TAGS=`$NTX tag`
assert "$TAGS" "*todo*"
assert "$TAGS" "*ntx*"
assert "$TAGS" "*pacman*"
assert "$TAGS" "*COW*"
assert "$TAGS" "*unix*"

# Clean up after ourselves.
rm -r $NTXROOT
rm $EDIT
