#!/bin/bash

# Use this script to run checkpatch on argument file only

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
$BUILDDIR/checkpatch/checkpatch.pl --color=never --emacs --terse --no-tree --max-line-length=120 -f $1
