#!/bin/bash

set -e

usage()
{
    echo "Usage: tags.sh [options] --root directory"
    echo
    echo "Options:"
    echo "    [--etags]  Generate Emacs TAGS file"
    echo "    [--cscope] Generate scope file"
    echo "    [--help]   Show help message"
    echo "    [-f|-o]    TAGS file name"

    exit 1
}

out=TAGS
cscope=0
etags=0
ROOT=../../src
FILES=()

while [ $# -gt 0 ]; do
    key=$1
    case $key in
        --root)
            ROOT="$2"
	    shift
	    shift
            ;;
        --etags)
            etags=1
            shift
            ;;
	--cscope)
	    cscope=1
	    shift
	    ;;
	-f|-o)
	    out=$2
	    shift
	    shift
	    ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

if [ $cscope == 1 ]; then
    echo "Generating cscope database is currently not supported"
fi

if [ $etags == 1 ]; then
    echo "Generating Emacs TAGS file $out..."
    ALL_FILES=$(git ls-files --exclude-standard --full-name $ROOT)
    BASE_DIR=$(readlink -e ../../)
    for item in $ALL_FILES;
    do
	FILES=("${FILES[@]}$BASE_DIR/$item\n")
    done
    echo -e ${FILES[@]} | ctags --totals -e -L - -f $out
fi
