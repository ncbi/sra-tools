#!/usr/bin/env bash

# the goal of this test is to verify that sam-dump produces a '*' aka star
# in the quality column of its SAM-output, if the cSRA-file has no quality
# values ( aka all elements in the quality-column are of value 255 in a row )
#
# the test uses the short count_qual.py - python-script
# to count the lines with and without qualities
#
# the test also uses the sam-factory-tool to produce a random cSRA-object
# to be used in this test ( no dependecies on production-runs ! )
#
# the test also depends on the bam-load-tool and kar-tool to produce a cSRA-object
#

exit 0

set -e

source ./check_bin_tools.sh $1 $2

print_verbose "testing for '*' in the output of sam-dump if input has not quality"
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
# the sam-factory-tool via stdin to produce 2 alignment-pairs
# and one unaligned record, in total 5 entries, all without qualities
$SAMFACTORY << EOF
r:type=random,name=R1,length=6000
ref-out:$RNDREF
sam-out:$RNDSAM
p:name=A,qual=*,repeat=2
p:name=A,qual=*,repeat=2
u:name=U1,len=44,qual=*
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
#run sam-dump, and let it produce SAM-output
SAM_DUMP_OUT="sam_dump_out.SAM"

#remove the SAM-file with qualities if it already exists...
if [[ -f "$SAM_DUMP_OUT" ]]; then
    rm "$SAM_DUMP_OUT"
fi

#run sam-dump without any options, produce the SAM-file with qualities
$SAMDUMP -u $RNDCSRA > $SAM_DUMP_OUT

#now use the python-script to count the lines with and without qualities
COUNTS=`cat $SAM_DUMP_OUT | ./count_qual.py`
print_verbose "counting lines in output of sam-dump:"
WQ_F1=`echo "$COUNTS" | cut -f1`
if [[ "$WQ_F1" -gt "0" ]]; then
    echo "count of lines with qualities, needs to be == 0"
    echo "but it is not, it is >$WQ_F1<"
    exit 3
else
    print_verbose "count of lines with qualities, needs to be == 0"
    print_verbose "...and it is!"
fi
WQ_F2=`echo "$COUNTS" | cut -f2`
if [[ "$WQ_F2" -eq "0" ]]; then
    echo "count of lines without qualities, needs to be > 0"
    echo "but it is not, it is >$WQ_F2<"
    exit 3
else
    print_verbose "count of lines without qualities, needs to be > 0"
    print_verbose "...and it is! ( value = $WQ_F2 )"
fi

#we do not need the SAM-file with qualities any more...
#we also do not need the random cSRA-object any more ...
rm "$SAM_DUMP_OUT" "$RNDCSRA"

print_verbose "success!"
print_verbose -e "--------\n"
