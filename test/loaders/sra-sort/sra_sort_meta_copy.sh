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

source ./check_bin_tools.sh $1 $3

VDB_INCDIR="$2"

print_verbose "testing sra-sort with post-sort metadata-copy"
print_verbose "---------------------------------------------"

#------------------------------------------------------------
#create a tempp. config-file
cat << EOF > tmp.kfg
/vdb/schema/paths = "${VDB_INCDIR}"
/LIBS/GUID = "8test002-6abf-47b2-bfd0-test-sra-sort"
EOF

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

ORG_CSRA="org_csra"

#we perform a bam-load into $ORG_CSRA
source ./sam_to_csra.sh $RNDSAM $RNDREF $ORG_CSRA
rm $RNDSAM $RNDREF

#now we have a cSRA-file ( actually a directory ) called org_csra

#------------------------------------------------------------
#run sra-stat on the original, store the xml-output in before.txt
#in the piped output remove the size-value with grep
$SRASTAT -sx ./$ORG_CSRA | grep -vE "(<Size value)" > before.xml

SORTED_CSRA="sorted_csra"

#run sra-sort on ORGCSRA, produce
#echo $SRASORT -f ./$ORG_CSRA ./$SORTED_CSRA
$SRASORT -f ./$ORG_CSRA ./$SORTED_CSRA

#run sra-stat on the sorted copy, store the xml-output in after.txt
#fix the name-difference
#in the piped output remove the size-value with grep
$SRASTAT -sx ./$SORTED_CSRA | sed 's/sorted_csra/org_csra/' | grep -vE "(<Size value)" > after.xml

diff -s before.xml after.xml

#we do not need the CSRA-objects any more ...
#we also do not need the xml-output(s) of sra-stat any more ...
rm -rf "${ORG_CSRA}" "${ORG_CSRA}.md5" "${SORTED_CSRA}" before.xml after.xml tmp.kfg
