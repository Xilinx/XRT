#!/bin/bash

set -e

usage()
{
    echo "Usage: tags.sh [options] --project <compile_commands.json>"
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

while [ $# -gt 0 ]; do
    key=$1
    case $key in
        --project)
            project="$2"
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
    echo "Generating Emacs TAGS file for XRT userspace code..."
    grep \"file\": $project | awk '{print $2}' | sed 's/\"//g' | ctags --totals -e -L - -f user.TAGS
fi

XOCL_FILES=$(git ls-files --full-name ../../src/runtime_src/core/pcie/driver/linux/xocl)
ZOCL_FILES=$(git ls-files --full-name ../../src/runtime_src/core/edge/drm/zocl)
BASE_DIR=$(readlink -e ../../)

FILES=()

for item in $XOCL_FILES;
do
    FILES=("${FILES[@]}$BASE_DIR/$item\n")
done

for item in $ZOCL_FILES;
do
    FILES=("${FILES[@]}$BASE_DIR/$item\n")
done

echo "Generating Emacs TAGS file for XRT Linux driver code..."
echo -e ${FILES[@]} | ctags --totals -e -L - -f kernel.TAGS

ctags -e --etags-include="$PWD/user.TAGS" --etags-include="$PWD/kernel.TAGS" -f $out
