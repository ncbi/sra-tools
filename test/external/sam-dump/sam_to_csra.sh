#!/usr/bin/env bash

# common helper script to check load a SAM-file with bam-load and produce an archive
# to be called by other scripts...
#

#------------------------------------------------------------
#load the random SAM-file with bam-load into a cSRA-object

set -e

SAM_SRC="$1"
REF_FILE="$2"
CSRA="$3"
CSRA_DIR="${CSRA}_dir"

#if the cSRA-object-directory alread exists, remove it
if [[ -d "$CSRA_DIR" ]]; then
    chmod +wr "$CSRA_DIR"
    rm -rf "$CSRA_DIR"
fi

$BAMLOAD $SAM_SRC --ref-file $REF_FILE --output $CSRA_DIR

#check if the random cSRA-object has been produced
if [[ ! -d "$CSRA_DIR" ]]; then
    echo "$CSRA_DIR not produced"
    exit 3
fi

print_verbose "cSRA-object ( directory ) produced!"

#------------------------------------------------------------
#package the cSRA-directory into single file

#if the cSRA-object alread exists, remove it
if [[ -f "$CSRA" ]]; then
    rm "$CSRA" $CSRA.md5
fi

#perform the packing via kar
$KAR -c $CSRA -d $CSRA_DIR

#check if the cSRA-object has been produced
if [[ ! -f "$CSRA" ]]; then
    echo "$CSRA not produced"
    exit 3
fi

#we do not need the cSRA-directory object any more
chmod +wr "$CSRA_DIR"
rm -rf $CSRA_DIR $CSRA.md5

print_verbose "cSRA-object ( single-file ) produced!"
