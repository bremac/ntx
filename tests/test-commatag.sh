
A="Hello, World!

Note contents don't matter, but we'd better ensure that this note is
valid!"

ed_write "$A"
V=`_ntx $EDIT add dwm,status`
Ai=`echo $V | cut -b 1-4`
$NTX tag $Ai dwm status todo
assert commatag "`$NTX tag `" "dwm
status
todo"
