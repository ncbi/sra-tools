#!/bin/bash

P_NM=`basename $0`

E_TAG="--extract"
C_TAG="--create"

usage () 
{
    cat << EOF

That utility will create kart file or extract data from it.

Syntax: $P_NM action source [ destination ]

Where:

         action - one of two "$E_TAG" or "$C_TAG"
         source - source text or cart file
    destination - destination text or cart file. Optional parameter.
                  if it is omitted, script will add or cut ".krt" 
                  from source file.

EOF

}

####
####
## Parsing Args
##
ACT_P=""
EXT_P="_do_extract"
CRE_P="_do_create"

SRC_F=""
SRC_S=0

DST_F=""
DST_S=0

if [ $# -ne 2 -a $# -ne 3 ]
then
    echo ERROR: invalid arguments passed
    usage
    exit 1
fi

case $1 in
    $E_TAG)
        ACT_P=$EXT_P
        ;;
    $C_TAG)
        ACT_P=$CRE_P
        ;;
    *)
        echo ERROR: invalid action parameter passed
        usage
        exit 1
        ;;
esac

SRC_F=$2
SRC_S=`stat -c "%s" $SRC_F 2>/dev/null`
if [ $? -ne 0 ]
then
    echo ERROR: can not stat file \'$SRC_F\'
    exit 1
fi

DST_F=$3

####
####
## Processing
##

MAGIC="ncbikart"

_do_extract ()
{
    if [ -z "$SRC_F" ]
    then
        echo "ERROR: undefined 'source' file"
        exit 1
    fi

    if [ -z "$DST_F" ]
    then
        DST_F=`dirname $SRC_F`/`basename $SRC_F .krt`.txt
    fi

    cat <<EOF
Extracting kart information
from: $SRC_F
  to: $DST_F
EOF
    if [ -f "$DST_F" ]
    then
        echo "Removing baaad file '$DST_F'"
        rm -f $DST_F
        if [ $? -ne 0 ]
        then
            echo "ERROR: Can not remove file '$DST_F'"
            exit 1
        fi
    fi

    tail -c $(( $SRC_S - ${#MAGIC} )) $SRC_F | gunzip -c >$DST_F

        ## There are header and footer, so ... simple check
    LIN_Q=`cat $DST_F | wc -l`
    if [ "$LIN_Q" -lt 2 ]
    then
        echo "WARNING: Invalid kart file format"
    else
        echo "EXTRACTED: $(( $LIN_Q - 1 )) entries"
    fi
}

_do_create ()
{
    if [ -z "$SRC_F" ]
    then
        echo "ERROR: undefined 'source' file"
        exit 1
    fi

    if [ -z "$DST_F" ]
    then
        DST_F=`dirname $SRC_F`/`basename $SRC_F .txt`.krt
    fi

    cat <<EOF
Creating kart file
from: $SRC_F
  to: $DST_F
EOF
    if [ -f "$DST_F" ]
    then
        echo "Removing baaad file '$DST_F'"
        rm -f $DST_F
        if [ $? -ne 0 ]
        then
            echo "ERROR: Can not remove file '$DST_F'"
            exit 1
        fi
    fi

    echo -n $MAGIC > $DST_F
    cat $SRC_F | gzip -c >> $DST_F
}

####
####
## Actrual processing is here
##

eval $ACT_P

if [ $? -ne 0 ]
then
    echo "ERROR: something come wrong :("
fi

echo "DONE OK"
