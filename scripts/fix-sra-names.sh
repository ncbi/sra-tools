#!/bin/sh

Help () {
echo "Usage: $0 <accession directory> [<accession directory>]..."
echo "Rename names of run files in <accession directory> to sra format"
echo "to be recognized by SRA tools."
echo
echo 'This script is intended to fix names of sra files'
echo 'that were downloaded without prefetch to be recognized by SRA tools.'
echo
echo 'Examples of directory with such files:'
echo '$ls SRR8639211'
echo 'SRR8639211.1'
echo
echo '$ls SRR8639212'
echo 'SRR8639212.1  SRR8639212.vdbcache.1'
echo
echo 'Script should be executed'
echo 'in directory that contains directories SRR8639211 [and SRR8639212].'
echo
echo 'Usage of fix-sra-names.sh:'
echo '$ls -d SRR8639211 SRR8639212'
echo 'SRR8639211 SRR8639212'
echo 'sh fix-sra-names.sh SRR8639211 SRR8639212'
echo
echo 'After fix-sra-names.sh was executed'
echo 'you can run SRA tools from the same curerent directory'
echo 'using accession ac command line argument:'
echo '$ls SRR8639211'
echo 'SRR8639211.1'
echo '$sh fix-sra-names.sh SRR8639211'
echo '$sam-dump SRR8639211'
echo '   # Now sam-dump will find run files located in SRR8639211 and use them.'
}

Fix_sra_names () {
    ARG=
    [ "$VERBOSE" != "" ] && ARG="-v"

    echo "$acc"

    DF="$acc/$acc.sra"
    DV="$acc/$acc.sra.vdbcache"

    if [ ! -d "$acc" ] ; then
        echo "$acc dir not found"
        return 1
    fi

    if [ -f "$DF" ] ; then
        echo "$DF found"
        return 2
    fi

    if [ -f "$DV" ] ; then
        echo "$DV found"
        return 2
    fi

    VERS=
    for (( I=1; ; I++ )) ; do
        SF="$acc/$acc.$I"
        if [ -f "$SF" ] ; then
            [ "$VERBOSE" != "" ] && echo "$SF found"
            VERS=$I
        else
            [ "$VERBOSE" != "" ] && echo "$SF not found"
            break
        fi
    done
    if [ "$VERS" == "" ] ; then
        echo "$SF not found"
        return 3
    fi

    SF="$acc/$acc.$VERS"
    [ "$VERBOSE" != "" ] && echo mv "$ARG" "$SF" "$DF"
    mv $ARG "$SF" "$DF"

    SV="$acc/$acc.vdbcache.$VERS"
    if [ -f "$SV" ] ; then
        [ "$VERBOSE" != "" ] && echo mv "$ARG" "$SV" "$DV"
        mv $ARG "$SV" "$DV"
    fi
}

if [ $# -eq 0 ] ; then
    Help
    exit 1
fi

for acc in "$@" ; do
	Fix_sra_names "$acc"
    echo
done
exit 0
