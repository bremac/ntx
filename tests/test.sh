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
  if [[ $2 = $3 ]]; then
    return
  else
    echo "Assertion $1 failed: '$2' = '$3'"
    die
  fi
}


# Set up the notes through a series of clever hacks.
ed_write "$A"
V=`_ntx $EDIT add soulfu git todo`
Av="Create a community soulfu git repository for the *nix p..."
Ai=`echo $V | cut -b 1-4`
assert add-1 "`echo $V | cut -b 6-`" "$Av"

ed_write "$B"
V=`_ntx $EDIT add ntx todo`
Bv="Add deletion support to ntx via a tagged/ directory."
Bi=`echo $V | cut -b 1-4`
assert add-2 "`echo $V | cut -b 6-`" "$Bv"

ed_write "$C"
V=`_ntx $EDIT add pacman COW todo`
Cv="Complete the Copy-On-Write pacman pseudo-FS/database."
Ci=`echo $V | cut -b 1-4`
assert add-3 "`echo $V | cut -b 6-`" "$Cv"

# Test listing notes - FIFO ordering is assumed.
assert list-1 "`$NTX list`" "$Ai$TAB$Av
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert list-2 "`$NTX list todo`" "$Ai$TAB$Av
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert list-3 "`$NTX list todo git`" "$Ai$TAB$Av"
assert list-4 "`$NTX list pacman ntx`" ""

# Test listing tags. Unfortunately, can't guarantee the order.
TAGS=`$NTX tag`
assert tag-1a "$TAGS" "*soulfu*"
assert tag-1b "$TAGS" "*git*"
assert tag-1c "$TAGS" "*todo*"
assert tag-1d "$TAGS" "*ntx*"
assert tag-1e "$TAGS" "*pacman*"
assert tag-1f "$TAGS" "*COW*"

# Test per-tag listing. Here, we _can_ test for a definite set, and order.
assert pertag-1 "`$NTX tag $Ai`" "soulfu
git
todo"
assert pertag-2 "`$NTX tag $Bi`" "ntx
todo"
assert pertag-3 "`$NTX tag $Ci`" "pacman
COW
todo"

# Test per-tag retagging. Again, we test for a defined set and order.
$NTX tag $Ai soulfu git todo unix

assert retag-1 "`$NTX tag $Ai`" "soulfu
git
todo
unix"
$NTX tag $Ci pacman todo COW unix
assert retag-2 "`$NTX tag $Ci`" "pacman
todo
COW
unix"

# Re-test above tag tests.
TAGS=`$NTX tag`
assert retag-3a "$TAGS" "*soulfu*"
assert retag-3b "$TAGS" "*git*"
assert retag-3c "$TAGS" "*todo*"
assert retag-3d "$TAGS" "*ntx*"
assert retag-3e "$TAGS" "*pacman*"
assert retag-3f "$TAGS" "*COW*"
assert retag-3g "$TAGS" "*unix*"

# Re-test listing notes for unix.
assert retag-4 "`$NTX list unix`" "$Ai$TAB$Av
$Ci$TAB$Cv"

# Test removing a tag and altering one.
$NTX tag $Bi ntx done
$NTX tag $Ci pacman COW unix

# Re-test above tags tests.
TAGS=`$NTX tag`
assert retag-5a "$TAGS" "*soulfu*"
assert retag-5b "$TAGS" "*git*"
assert retag-5c "$TAGS" "*todo*"
assert retag-5d "$TAGS" "*ntx*"
assert retag-5e "$TAGS" "*pacman*"
assert retag-5f "$TAGS" "*COW*"
assert retag-5g "$TAGS" "*done*"
assert retag-5h "$TAGS" "*unix*"

assert retag-6 "`$NTX list todo`" "$Ai$TAB$Av"
assert retag-7 "`$NTX list done`" "$Bi$TAB$Bv"
assert retag-8 "`$NTX list unix`" "$Ai$TAB$Av
$Ci$TAB$Cv"

# Reset the tags back to normal for later tests.
$NTX tag $Bi ntx todo
$NTX tag $Ci pacman todo COW unix

# Test altering a note - Edit A to be equal to b.
ed_write "$B"
V=`_ntx $EDIT edit $Ai`
assert edit-1 "`echo $V | cut -b 6-`" "$Bv"

# Re-test to ensure the index and tags have changed.
assert edit-2 "`$NTX list`" "$Ai$TAB$Bv
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert edit-3 "`$NTX list todo git`" "$Ai$TAB$Bv"

# Test resetting A to itself via stdin.
V=`echo $A | $NTX edit $Ai`
assert edit-4 "`echo $V | cut -b 6-`" "$Av"

# Re-test to ensure the index and tags have changed.
assert edit-5 "`$NTX list`" "$Ai$TAB$Av
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert edit-6 "`$NTX list todo git`" "$Ai$TAB$Av"

# Test deleting a note - Delete A from the set.
$NTX rm $Ai

# Re-test listings and tags.
assert del-1 "`$NTX list`" "$Bi$TAB$Bv
$Ci$TAB$Cv"
assert del-2 "`$NTX list todo`" "$Bi$TAB$Bv
$Ci$TAB$Cv"
assert del-3 "`$NTX list git`" ""

TAGS=`$NTX tag`
assert del-4a "$TAGS" "*todo*"
assert del-4b "$TAGS" "*ntx*"
assert del-4c "$TAGS" "*pacman*"
assert del-4d "$TAGS" "*COW*"
assert del-4e "$TAGS" "*unix*"

# Clean up after ourselves.
rm -r $NTXROOT
rm $EDIT

# Run regression tests as necessary.
for regressiontest in "`pwd`/test-*.sh"; do
  . $regressiontest
  rm -r $NTXROOT
  rm $EDIT
done
