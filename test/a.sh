#!/bin/bash
# Script to add a note, used as an EDITOR replacement for testing.

cat > $1 <<EOF
Create a community soulfu git repository for the *nix port.

Unfortunately, the current soulfu 'development' model needs work:
Changes made are not committed to CVS, and developer time is wasted
recreating works already created and documented by others. The
gains from CVS are obviously paltry compared to those offered by
a more distributed model, given the amount of work which is required,
so a mob branch will be opened to all in order to prompt the fastest
progress possible.

Contingent on this is liberating the engine source under the GPL, so
that works can be _legally_ distributed, instead of sneaking around
and hoping to find a loophole.
EOF