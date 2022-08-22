#!/usr/bin/env bash

# the goal of this test is to verify that sam-dump does work with and
# without the 'omit-quality' option
#
# the test uses the short count_qual.py - python-script
# to count the lines with and without qualities
#
# the test also uses the sam-factory-tool to produce a random cSRA-object
# to be used in this test ( no dependecies on production-runs ! )
#
# the test also depends on the bam-load-tool to produce a cSRA-object
#

BINDIR="$1"
SAMDUMP="${BINDIR}/sam-dump"
BAMLOAD="${BINDIR}/bam-load"

echo "testing the omit-qual - option for sam-dump"
echo "-------------------------------------------"

set -e

#------------------------------------------------------------
# let us check if the tools we depend on do exist
if [[ ! -x "$SAMDUMP" ]]; then
    echo "$SAMDUMP - executable not found"
	exit 3
fi

if [[ ! -x "$BAMLOAD" ]]; then
    echo "$BAMLOAD - executable not found"
	exit 3
fi

echo "sam-dump and bam-load executables found!"

#------------------------------------------------------------
#produce ( compile ) the sam-factory-tool:

SAMFACTORY="./sam-factory"
SAMFACTORYSRC="./$SAMFACTORY.cpp"

#check if we have to source-code for the sam-factory tool
if [[ ! -f "$SAMFACTORYSRC" ]]; then
    echo "$SAMFACTORYSRC not found"
    exit 3
fi

#if the sam-factory tool alread exists, remove it
if [[ -f "$SAMFACTORY" ]]; then
    rm -f "$SAMFACTORY"
fi

#build the sam-factory tool
g++ $SAMFACTORYSRC -o $SAMFACTORY
if [[ ! -x "$SAMFACTORY" ]]; then
    echo "$SAMFACTOR could not be build"
    exit 3
fi

echo "sam-factory-tool produced!"

#------------------------------------------------------------
#produce a random sam-file

RNDSAM="rnd_sam.SAM"
RNDREF="rnd-ref.fasta"

#if the random sam-file alread exists, remove it
if [[ -f "$RNDSAM" ]]; then
    rm -f "$RNDSAM"
fi

#if the random reference alread exists, remove it
if [[ -f "$RNDREF" ]]; then
    rm -f "$RNDREF"
fi

#with the help of HEREDOC we pipe the configuration into
# the sam-factory-tool via stdin to produce 20 alignment-pairs
cat << EOF | $SAMFACTORY
r:type=random,name=R1,length=6000
ref-out:$RNDREF
sam-out:$RNDSAM
p:name=A,repeat=20
p:name=A,repeat=20
EOF

#check if the random sam-file has been produced
if [[ ! -f "$RNDSAM" ]]; then
    echo "$RNDSAM not produced"
    exit 3
fi

#check if the random reference has been produced
if [[ ! -f "$RNDREF" ]]; then
    echo "$RNDREF not produced"
    exit 3
fi

echo "random SAM-file produced!"

#we do not need the sam-factory-tool any more...
rm $SAMFACTORY

#------------------------------------------------------------
#load the random SAM-file with bam-load into a cSRA-object

RNDCSRA="rnd_csra"

#if the random cSRA-object alread exists, remove it
if [[ -d "$RNDCSRA" ]]; then
    chmod +wr "$RNDCSRA"
    rm -rf "$RNDCSRA"
fi

$BAMLOAD $RNDSAM --ref-file $RNDREF --output $RNDCSRA

#check if the random cSRA-object has been produced
if [[ ! -d "$RNDCSRA" ]]; then
    echo "$RNDCSRA not produced"
    exit 3
fi

echo "random cSRA-object produced!"

#we do not need the random SAM-file and the random ref-file any more
rm $RNDSAM $RNDREF

#------------------------------------------------------------
#run sam-dump, and let it produce SAM-output with qualities
SAM_W="with_qual.SAM"

#remove the SAM-file with qualities if it already exists...
if [[ -f "$SAM_W" ]]; then
    rm "$SAM_W"
fi

#run sam-dump without any options, produce the SAM-file with qualities
$SAMDUMP $RNDCSRA > $SAM_W

#now use the python-script to count the lines with qualities
COUNTS_W=`cat $SAM_W | ./count_qual.py`
echo "counting lines in output of sam-dump with qualities (default):"
echo "  count of lines with qualities, needs to be > 0"
WQ_F1=`echo "$COUNTS_W" | cut -f1`
if [[ ! "$WQ_F1" -gt "0" ]]; then
    echo "  but it is not, it is >$WQ_F1<"
    exit 3
else
    echo "  ...and it is!"
fi
echo "  count of lines without qualities, needs to be == 0"
WQ_F2=`echo "$COUNTS_W" | cut -f2`
if [[ ! "$WQ_F2" -eq "0" ]]; then
    echo "  but it is not, it is >$WQ_F2<"
    exit 3
else
    echo "  ...and it is!"
fi

#we do not need the SAM-file with qualities any more...
rm $SAM_W


#------------------------------------------------------------
#run sam-dump, and let it produce SAM-output without qualities

#run sam-dump, and let it produce SAM-output with qualities
SAM_O="without_qual.SAM"

#remove the SAM-file without qualities if it already exists...
if [[ -f "$SAM_O" ]]; then
    rm "$SAM_O"
fi

#run sam-dump with the --omit-quality option, produce the SAM-file without qualities
$SAMDUMP $RNDCSRA --omit-quality > $SAM_O

COUNTS_O=`cat $SAM_O | ./count_qual.py`
echo "counting lines in output of sam-dump without qualities ( --omit-quality ):"
echo "  count of lines with qualities, needs to be == 0"
NQ_F1=`echo "$COUNTS_O" | cut -f1`
if [[ ! "$NQ_F1" -eq "0" ]]; then
    echo " but it is not, it is >$NQ_F1<"
    exit 3
else
    echo "  ...and it is!"
fi
echo "  count of lines without qualities, needs to be > 0"
NQ_F2=`echo "$COUNTS_O" | cut -f2`
if [[ ! "$NQ_F2" -gt "0" ]]; then
    echo "  but it is not, it is >$NQ_F2<"
    exit 3
else
    echo "  ...and it is!"
fi

#we do not need the SAM-file without qualities any more ...
rm $SAM_O

#we also do not need the random cSRA-object any more ...
chmod +wr "$RNDCSRA"
rm -rf "$RNDCSRA"

echo "success!"
echo -e "--------\n"
