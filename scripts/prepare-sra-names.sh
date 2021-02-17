#!/bin/bash

Help () {
echo "Usage: $0 <accession> [<accession>]..."
echo 'Rearrange run files in current directory'
echo 'by creating accession directories and moving files there.'
echo
echo 'This script is intended to rearrange sra files'
echo 'that were downloaded without prefetch.'
echo 'Next step: run "fix-sra-names.sh <accession> [<accession>]..."'
echo
echo 'Examples of directory with such files:'
echo '$ls GSE118828'
echo 'SRR7725681.1 SRR7725682.1 SRR7725682.vdbcache.1 SRR7725683.1 SRR7725684.1'
echo '...'
echo
echo 'Script should be executed in directory that contains SRR* files.'
echo
echo "Usage of $0:"
echo "bash $0 SRR7725681 SRR7725682 SRR7725683 SRR7725684"
echo
echo 'The script will create a directory for accession'
echo 'and move files related to this accession there.'
echo
echo "After $0 was executed -"
echo 'run fix-sra-names.sh from the same directory with the same argument list.'
}

Prepare_sra_name () {
    ARG=
    [ "$VERBOSE" != "" ] && ARG="-v"

    echo "$acc"

    if [[ "$acc" = */* ]] ; then
        echo "$acc is path - accession expected"
        return 1
    fi

    if [[ "$acc" != [DES]RR+([0-9]) ]] ; then
        echo "$acc is not recognized as run accession"
        return 2
    fi

    if ls $acc.* 1> /dev/null 2>&1; then
        if [ ! -d "$acc" ] ; then
            mkdir $ARG "$acc"
        fi
        mv $ARG $acc.* "$acc"
    else
        echo "$acc.* do not exist - nothing to do"
        return 0
    fi
}

if [ $# -eq 0 ] ; then
    Help
    exit 1
fi

for acc in "$@" ; do
	Prepare_sra_name "$acc"
    echo
done
exit 0
