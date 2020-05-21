#!/bin/bash

# Use this script to run checkpatch on argument file only

BUILDDIR=$(readlink -f $(dirname ${BASH_SOURCE[0]}))
if [ ! -x $BUILDDIR/checkpatch/checkpatch.pl ]; then
    curl -o $BUILDDIR/checkpatch/checkpatch.pl "https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/plain/scripts/checkpatch.pl?h=v4.14.180"
    chmod +x $BUILDDIR/checkpatch/checkpatch.pl
fi

$BUILDDIR/checkpatch/checkpatch.pl --color=never --emacs --terse --no-tree --max-line-length=120 -f $1
