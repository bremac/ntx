#!/bin/bash
# Testing script to ensure that ntx continues functioning correctly on unix.
# Add each bug as it is found to this, to form a set of regression tests.
# They'll hardly need to be comprehensive anyway, given such a small codebase.

NTX=../src/ntx
TAB="	"
EDIT=editor.sh
export NTXROOT=`pwd`/"ntx_test"

if [ -d $NTXROOT ]; then
  rm -r $NTXROOT
fi

# Used to manipulate ntx add/edit.
function _ntx {
  export EDITOR=`pwd`/$1
  OP=$2
  shift; shift
  $NTX $OP $@
}

# Used to generate a script for EDITOR.
function ed_write {
  SRC=test_notes/$1.txt

  echo "#!/bin/bash" > $EDIT
  echo 'cat > $1 <<EOF' >> $EDIT
  cat test_notes/$1.txt >> $EDIT
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
ed_write a
V=`_ntx $EDIT add soulfu git todo`
Av="Create a community soulfu git repository for the *nix p..."
Ai=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "$Av"

ed_write b
V=`_ntx $EDIT add ntx todo`
Bv="Add deletion support to ntx via a tagged/ directory."
Bi=`echo $V | cut -b 1-4`
assert "`echo $V | cut -b 6-`" "$Bv"

ed_write c
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
assert "`$NTX list pacman ntx`" \
"No notes exist in the intersection of those tags."

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

# Test removing a tag.
# XXX

# Test altering a note - Edit A to be equal to b.
ed_write b
V=`_ntx $EDIT edit $Ai`
assert "`echo $V | cut -b 6-`" "$Bv"

# Re-test to ensure the index and tags have changed.
assert "`$NTX list`" "$Ai$TAB$Bv
$Bi$TAB$Bv
$Ci$TAB$Cv"
assert "`$NTX list todo git`" "$Ai$TAB$Bv"

# Test deleting a note - Delete A from the set.
# Re-test listings and tags.

# Clean up after ourselves.
rm -r $NTXROOT
rm $EDIT
