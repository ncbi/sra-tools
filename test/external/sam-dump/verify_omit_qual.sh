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
# the test also depends on the bam-load-tool and kar-tool to produce a cSRA-object
#

set -e

source ./check_bin_tools.sh $1 $2 $3

print_verbose "testing the omit-qual - option for sam-dump"
print_verbose "-------------------------------------------"

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
$SAMFACTORY << EOF
r:type=random,name=R1,length=6000
ref-out:$RNDREF
sam-out:$RNDSAM
p:name=A,repeat=2000
p:name=A,repeat=2000
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

print_verbose "random SAM-file produced!"

RNDCSRA="rnd_csra"
source ./sam_to_csra.sh $RNDSAM $RNDREF $RNDCSRA
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
print_verbose "counting lines in output of sam-dump with qualities (default):"
WQ_F1=`echo "$COUNTS_W" | cut -f1`
if [[ ! "$WQ_F1" -gt "0" ]]; then
    echo "T1:count of lines with qualities, needs to be > 0"
    echo "T1:but it is not, it is >$WQ_F1<"
    exit 3
else
    print_verbose "count of lines with qualities, needs to be > 0"
    print_verbose "...and it is!"
fi
WQ_F2=`echo "$COUNTS_W" | cut -f2`
if [[ ! "$WQ_F2" -eq "0" ]]; then
    echo "T1:count of lines without qualities, needs to be == 0"
    echo "T1:but it is not, it is >$WQ_F2<"
    exit 3
else
    print_verbose "count of lines without qualities, needs to be == 0"
    print_verbose "...and it is!"
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
print_verbose "counting lines in output of sam-dump without qualities ( --omit-quality ):"
NQ_F1=`echo "$COUNTS_O" | cut -f1`
if [[ ! "$NQ_F1" -eq "0" ]]; then
    echo "T2:count of lines with qualities, needs to be == 0"
    echo "T2:but it is not, it is >$NQ_F1<"
    exit 3
else
    print_verbose "count of lines with qualities, needs to be == 0"
    print_verbose "...and it is!"
fi
NQ_F2=`echo "$COUNTS_O" | cut -f2`
if [[ ! "$NQ_F2" -gt "0" ]]; then
    echo "T2:count of lines without qualities, needs to be > 0"
    echo "T2:but it is not, it is >$NQ_F2<"
    exit 3
else
    print_verbose "count of lines without qualities, needs to be > 0"
    print_verbose "...and it is!"
fi

#we do not need the SAM-file without qualities any more ...
rm $SAM_O

#we also do not need the random cSRA-object any more ...
rm "$RNDCSRA"

print_verbose "success!"
print_verbose -e "--------\n"
