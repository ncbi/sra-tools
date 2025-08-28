#!/bin/bash
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# =============================================================================$

#NOT_ALL=yes

bin_dir=$1
prefetch=$2
TESTBINDIR=$3
VERBOSE=$4

verboseN=0
if [ "$VERBOSE" != "" ]; then verboseN=1; fi
PREFETCH=$bin_dir/$prefetch

#echo cmake_source_dir=${cmake_source_dir}

TEMPDIR=.

echo Testing ${prefetch} from ${bin_dir}

# rm -rf actual
# mkdir -p actual

# echo makedb:
# rm -rf data; mkdir -p data; ${vdb_dump_makedb}

# echo output format
# output=$(NCBI_SETTINGS=/ ${bin_dir}/vdb-dump SRR056386 -R 1 -C READ -f tab > actual/1.0.stdout && diff expected/1.0.stdout actual/1.0.stdout)
# res=$?
# if [ "$res" != "0" ];
	# then echo "vdb_dump 1.0 FAILED, res=$res output=$output" && exit 1;
# fi

HTTP_URL=https://test.ncbi.nlm.nih.gov/home/about/contact.shtml
HTTPFILE=contact.shtml

PUBLIC=/repository/user/main/public
SDL=https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve
LARGE=fasp://aspera@demo.asperasoft.com:aspera-test-dir-large/100MB
SRA=https://sra-download.ncbi.nlm.nih.gov
SRAF=fasp://dbtest@sra-download.ncbi.nlm.nih.gov
KMER=${SRA}/traces/nannot01/kmer/000/390/GCA_000390265.1_R
KMERF=${SRAF}:data/sracloud/traces/nannot01/kmer/000/390/GCA_000390265.1_R
REFSEQ=${SRA}/traces/refseq/KC702174.1
REFSEQF=${SRAF}:data/sracloud/traces/refseq/KC702174.1
SRAC=SRR053325
SRR=${SRA}/sos2/sra-pub-run-15/${SRAC}/${SRAC}.4
SRR=$(NCBI_SETTINGS=/ ${bin_dir}/srapath ${SRAC})
INT=https://sra-download-internal.ncbi.nlm.nih.gov
DDB=https://sra-downloadb.be-md.ncbi.nlm.nih.gov
SRAI=fasp://dbtest@sra-download-internal.ncbi.nlm.nih.gov
SRRF=${SRAF}:data/sracloud/traces/sra57/SRR/000052/${SRAC}
SRRF=${SRAI}:data/sracloud/traces/sra57/SRR/000052/${SRAC}
WGS=${SRA}/traces/wgs03/WGS/AF/VF/AFVF01.1
WGSF=${SRAF}:data/sracloud/traces/wgs03/WGS/AF/VF/AFVF01.1

work_dir=$(pwd)
#echo WORK DIRECTORY: ${work_dir}

if [ "$NOT_ALL" == "" ]; then
# ##############################################################################
# echo s-option:
mkdir -p tmp
echo "repository/remote/main/SDL.2/resolver-cgi = \"$SDL\"" > tmp/t.kfg
echo "$PUBLIC/apps/sra/volumes/sraFlat = \"sra\""          >> tmp/t.kfg
echo "$PUBLIC/root = \"$work_dir/tmp\""                         >> tmp/t.kfg

# echo Downloading KART was disabled: need to have a public example of protected run or add support of public accessions
# rm -f tmp/sra/SRR0*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/${prefetch} kart.krt > /dev/null

# echo Downloading KART: ORDER BY SIZE / SHORT OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/${prefetch} kart.krt -os > /dev/null

# echo Downloading KART: ORDER BY KART / SHORT OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/${prefetch} kart.krt -ok > /dev/null

# echo Downloading KART: ORDER BY SIZE / LONG OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/${prefetch} kart.krt --order s > /dev/null

# echo Downloading KART: ORDER BY KART / LONG OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/${prefetch} kart.krt --order k > /dev/null

echo Downloading ACCESSION
rm -f tmp/sra/${SRAC}.sra
COMMAND\
="export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/; $PREFETCH $SRAC"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
output=\
$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/; $PREFETCH $SRAC)
if [ "$?" != "0" ]; then
    echo "Downloading ACCESSION FAILED, CMD=$COMMAND"; exit 119
fi
rm tmp/sra/${SRAC}.sra || exit 121

echo Downloading ACCESSION TO FILE: SHORT OPTION
rm -f tmp/1
COMMAND\
="export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/; $PREFETCH $SRAC -otmp/1"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
output=\
$(NCBI_SETTINGS=/ NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= VDB_CONFIG=`pwd`/tmp \
  $PREFETCH $SRAC -otmp/1)
if [ "$?" != "0" ]; then
    echo "Downloading ACCESSION TO FILE: SHORT OPTION FAILED, CMD=$COMMAND"
    exit 132
fi
rm tmp/1 || exit 134

echo Downloading ACCESSION TO FILE: LONG OPTION
rm -f tmp/2
COMMAND\
="export VDB_CONFIG=`pwd`/tmp;export NCBI_SETTINGS=/;$PREFETCH $SRAC --output-file tmp/1"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
output=\
$(NCBI_SETTINGS=/ NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= VDB_CONFIG=`pwd`/tmp \
  $PREFETCH $SRAC --output-file tmp/1)
if [ "$?" != "0" ]; then
    echo "Downloading ACCESSION TO FILE: LONG OPTION FAILED, CMD=$COMMAND"
    exit 145
fi
rm tmp/1 || exit 147

echo Downloading ACCESSIONS
rm -f tmp/sra/*
COMMAND\
="export VDB_CONFIG=`pwd`/tmp;export NCBI_SETTINGS=/; $PREFETCH $SRAC SRR045450"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
output=\
$(export VDB_CONFIG=`pwd`/tmp;export NCBI_SETTINGS=/; $PREFETCH $SRAC SRR045450)
if [ "$?" != "0" ]; then
    echo "Downloading ACCESSIONS FAILED, CMD=$COMMAND"; exit 157
fi
rm tmp/sra/$SRAC.sra tmp/sra/SRR045450.sra || exit 159

echo Testing download of multiple items to file
CMD="$PREFETCH SRR045450 $SRAC -o tmp/o"
COMMAND\
="export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/; $CMD"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
if $CMD 2> /dev/null; \
  then echo unexpected success when downloading multiple items to file; \
  exit 169; \
fi

# echo  version 1.0     >  tmp/k
# echo '0||SRR045450||' >> tmp/k
# echo '0||SRR053325||' >> tmp/k
# echo '$$end'          >> tmp/k

# if [ "${BUILD}" = "dbg" ]; then \
# echo Downloading TEXT KART is disabled, as well ; \
# if [ "${BUILD}" = "dg" ]; then \
      # echo Downloading TEXT KART && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/${prefetch} --text-kart tmp/k > /dev/null && \
      # \
      # echo Downloading TEXT KART: ORDER BY SIZE / SHORT OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/${prefetch} --text-kart tmp/k -os > /dev/null 2>&1 && \
      # \
      # echo Downloading TEXT KART: ORDER BY KART / SHORT OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/${prefetch} --text-kart tmp/k -ok > /dev/null 2>&1 && \
      # \
      # echo Downloading TEXT KART: ORDER BY SIZE / LONG OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/${prefetch} --text-kart tmp/k --order s > /dev/null && \
      # \
      # echo Downloading TEXT KART: ORDER BY KART / LONG OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/${prefetch} --text-kart tmp/k --order k > /dev/null ; \
  # fi \
  # fi

rm -r tmp*

echo truncated:
echo ${prefetch} correctly re-downloads incomplete files

#setup
mkdir -p tmp/sra || exit 215
echo "repository/remote/main/SDL.2/resolver-cgi = \"${SDL}\""> tmp/t.kfg \
    || exit 217
echo "${PUBLIC}/apps/sra/volumes/sraFlat = \"sra\""         >> tmp/t.kfg \
    || exit 219
echo "${PUBLIC}/root = \"$(pwd)/tmp\""                      >> tmp/t.kfg \
    || exit 221
#	@ ls -l tmp	@ cat tmp/t.kfg
#	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/vdb-config -on
#	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/srapath ${SRAC}
#	echo 669	perl check-size.pl 4	echo 671

# create truncated run
echo '123' > tmp/sra/${SRAC}.sra || exit 228
if perl check-size.pl 4 > /dev/null; \
    then echo "run should be truncated for this test"; exit 1; \
fi

# prefetch should detect truncated run and redownload it
COMMAND\
="VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ $PREFETCH $SRAC"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
VDB_CONFIG=`pwd`/tmp   NCBI_SETTINGS=/ $PREFETCH $SRAC > tmp/out || exit 237
if grep ": 1) '${SRAC}' is found locally" tmp/out ; \
    then echo "${prefetch} incorrectly stopped on truncated run"; exit 239; \
fi
#	@ wc tmp/sra/${SRAC}.sra	@ wc -c tmp/sra/${SRAC}.sra

# run should be complete
perl check-size.pl 4 || exit 244

# the second run of prefetch should find local file
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
VDB_CONFIG=`pwd`/tmp   NCBI_SETTINGS=/ $PREFETCH $SRAC > tmp/out || exit 248
grep -q ": 1) '${SRAC}' is found locally" tmp/out                || exit 249
perl check-size.pl 4                                             || exit 250

#cleanup
rm -r tmp                                                        || exit 253

echo kart: ##########################################################################
rm -frv tmp/* || exit 256
mkdir -p tmp  || exit 257
echo "repository/remote/main/SDL.2/resolver-cgi = \"${SDL}\"" > tmp/t.kfg \
    || exit 259

echo "Downloading --cart <kart> ordered by default (size)"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc --cart ../data/3-dbGaP-0.krt -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"| \
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc --cart ../data/3-dbGaP-0.krt -Cn" \
                                                    || exit 273
cd ${work_dir}                                      || exit 274
rm     tmp/SRR1219879/SRR1219879.sra*               || exit 275
rm     tmp/SRR1219880/SRR1219880.sra*               || exit 276
rm     tmp/SRR1257493/SRR1257493.sra*               || exit 277
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 278

echo "Downloading kart ordered by default (size)"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
                                                    || exit 291
cd ${work_dir}                                      || exit 292
rm     tmp/SRR1219879/SRR1219879.sra*               || exit 293
rm     tmp/SRR1219880/SRR1219880.sra*               || exit 294
rm     tmp/SRR1257493/SRR1257493.sra*               || exit 295
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 296

echo "Downloading kart ordered by size"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn" \
                                                    || exit 309
cd ${work_dir}                                      || exit 310
rm     tmp/SRR1219879/SRR1219879.sra*               || exit 311
rm     tmp/SRR1219880/SRR1219880.sra*               || exit 312
rm     tmp/SRR1257493/SRR1257493.sra*               || exit 313
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 314

echo "Downloading kart ordered by kart"
COMMAND\
="cd tmp &&printf \"ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                                    || exit 326
cd ${work_dir}                                      || exit 327
rm tmp/SRR1219879/SRR1219879.sra*                   || exit 328
rm tmp/SRR1219880/SRR1219880.sra*                   || exit 329
rm tmp/SRR1257493/SRR1257493.sra*                   || exit 330
rmdir tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493  || exit 331

rm -frv tmp/sra                                     || exit 333
echo "repository/remote/main/SDL.2/resolver-cgi = \"${SDL}\"" > tmp/t.kfg \
                                                                 || exit 335
echo "${PUBLIC}/apps/sra/volumes/sraFlat = \"sra\"" >> tmp/t.kfg || exit 337
echo "${PUBLIC}/root = \"$(pwd)/tmp\""              >> tmp/t.kfg || exit 338

echo "Downloading kart ordered by default (size) to user repo"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
                                      || exit 351
cd ${work_dir}                        || exit 352
rm    tmp/sra/SRR1219879.sra*         || exit 353
rm    tmp/sra/SRR1219880.sra*         || exit 354
rm    tmp/sra/SRR1257493.sra*         || exit 355
rmdir tmp/sra                         || exit 356

echo "Downloading kart ordered by size to user repo"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn" \
                                      || exit 369
cd ${work_dir}                        || exit 370
rm    tmp/sra/SRR1219879.sra*         || exit 371
rm    tmp/sra/SRR1219880.sra*         || exit 372
rm    tmp/sra/SRR1257493.sra*         || exit 373
rmdir tmp/sra                         || exit 374

echo "Downloading kart ordered by kart to user repo"
COMMAND\
="cd tmp &&printf \"ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                      || exit 387
cd ${work_dir}                        || exit 388
rm    tmp/sra/SRR1219879.sra*         || exit 389
rm    tmp/sra/SRR1219880.sra*         || exit 390
rm    tmp/sra/SRR1257493.sra*         || exit 391
rmdir tmp/sra                         || exit 392

echo '/tools/prefetch/download_to_cache = "false"' >> tmp/t.kfg || exit 394
echo "Downloading kart ordered by default (size) ignoring user repo"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
                                                    || exit 406
cd ${work_dir}                                      || exit 407
rm     tmp/SRR1219879/SRR1219879.sra*               || exit 408
rm     tmp/SRR1219880/SRR1219880.sra*               || exit 409
rm     tmp/SRR1257493/SRR1257493.sra*               || exit 410
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 411

echo "Downloading kart ordered by size ignoring user repo"
COMMAND\
="cd tmp &&printf \"ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn"
cd ${work_dir}                                      || exit 424
rm     tmp/SRR1219879/SRR1219879.sra*               || exit 425
rm     tmp/SRR1219880/SRR1219880.sra*               || exit 426
rm     tmp/SRR1257493/SRR1257493.sra*               || exit 427
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 428

echo "Downloading kart ordered by kart ignoring user repo"
COMMAND\
="cd tmp &&printf \"ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'\"|"\
"NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. "\
"perl ../check-prefetch-out.pl 0 $verboseN "\
"\"$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn\""
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
NCBI_SETTINGS=/ NCBI_VDB_QUALITY=R VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 $verboseN \
 "$PREFETCH --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                                   || exit 441
cd ${work_dir}                                     || exit 442
rm tmp/SRR1219879/SRR1219879.sra*                  || exit 443
rm tmp/SRR1219880/SRR1219880.sra*                  || exit 444
rm tmp/SRR1257493/SRR1257493.sra*                  || exit 444
rmdir tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 445

rm -r tmp                                          || exit 450


echo wgs:
echo Verifying ${prefetch} of runs with WGS references...

rm -fr tmp || exit 453
mkdir  tmp || exit 484

#pwd
COMMAND\
="cd tmp&&NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR619505 -fy"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR619505 -fy > /dev/null \
                                                                     || exit 460
cd ${work_dir}                                                       || exit 461
ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null || exit 462
rm -r tmp/S*                                                         || exit 463

echo '/repository/site/disabled = "true"'                 >> tmp/k   || exit 468
#echo '/libs/vdb/quality = "ZR"'                          >> tmp/k   || exit 465
#if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
#cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR619505 -fy > /dev/null \
#                                                                    || exit 468
#cd ${work_dir}                                                      || exit 469
#ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null|| exit 470
#rm -r tmp/[NS]*                                                     || exit 471

echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"' >> tmp/k  \
                                                                     || exit 474
echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
	                                                       >> tmp/k  || exit 476
echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k \
                                                                     || exit 478
printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd` >> tmp/k||exit 479
#cat tmp/k

if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR619505 -fy > /dev/null \
                                                                     || exit 485
cd ${work_dir}                                                       || exit 486
ls tmp/SRR619505/SRR619505.sra tmp/refseq/NC_000005.8    > /dev/null || exit 487
rm -r tmp/[rS]*                                                      || exit 488

echo prefetch run with AAAB01 WGS refseq to cache
COMMAND\
="cd tmp&&NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR353827 -fy"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR353827 -fy > /dev/null \
                                                                     || exit 495
cd ${work_dir}                                                       || exit 496
ls tmp/SRR353827/SRR353827.sralite tmp/wgs/AAAB01        > /dev/null || exit 497
rm -r tmp/[Sw]*                                                      || exit 498

echo prefetch run with AAAB01 WGS refseq to AD
echo '/tools/prefetch/download_to_cache = "false"'          >> tmp/k || exit 501
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR353827 -fy > /dev/null \
                                                                     || exit 503
ls SRR353827/SRR353827.sralite SRR353827/AAAB01          > /dev/null || exit 504
rm -r SRR353827                                                      || exit 505

COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR353827 -Sn"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/ VDB_CONFIG=k   $PREFETCH SRR353827 -Sn   > /dev/null || exit 510

COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info SRR353827|grep -q 'false,remote::http'"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/ VDB_CONFIG=k   ${bin_dir}/align-info SRR353827 \
	                                  | grep -q 'false,remote::http' || exit 516

COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH SRR353827"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/ VDB_CONFIG=k   $PREFETCH SRR353827       > /dev/null || exit 521

COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info SRR353827|grep -qv 'false,remote::http'"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/ VDB_CONFIG=k   ${bin_dir}/align-info SRR353827 \
	                                 | grep -qv 'false,remote::http' || exit 527

cd ${work_dir}                                                       || exit 529
rm -r tmp                                                            || exit 530


echo lots_wgs:
echo  Verifying ${prefetch} of run with a lot of WGS references to user repo...
rm -fr tmp                                                           || exit 535
mkdir  tmp                                                           || exit 536

echo '/repository/site/disabled = "true"'                    > tmp/k || exit 539
echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"' \
                                                            >> tmp/k || exit 541
echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
	                                                        >> tmp/k || exit 543
echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k \
                                                                     || exit 545
printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd`        >> tmp/k \
                                                                     || exit 547
echo cd tmp
cd tmp                                                               || exit 553

echo '      ...prefetching...'
COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH ERR3091357"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/   VDB_CONFIG=k $PREFETCH ERR3091357 > /dev/null \
                                                                     || exit 554

echo '      ...align-infoing...'
COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info ERR3091357|grep 'false,remote::http'"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/ VDB_CONFIG=k   ${bin_dir}/align-info ERR3091357 \
	                                     | grep 'false,remote::http' && exit 561
ls ERR3091357/ERR3091357.sralite wgs/JTFH01 refseq/KN707955.1 \
                                                         > /dev/null || exit 563
echo cd $work_dir
cd ${work_dir}                                                       || exit 564
rm -r tmp                                                            || exit 565

echo  Verifying ${prefetch} of a run with with a lot of WGS references to AD...
rm -fr tmp                                                           || exit 566
mkdir  tmp                                                           || exit 567

echo '/repository/site/disabled = "true"'                    > tmp/k || exit 581
echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"' \
                                                            >> tmp/k || exit 583
echo cd tmp
cd tmp                                                               || exit 585

echo '      ...prefetching...'
COMMAND="NCBI_SETTINGS=/ VDB_CONFIG=k $PREFETCH ERR3091357"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/          VDB_CONFIG=k $PREFETCH ERR3091357 >/dev/null|| exit 590

echo '      ...align-infoing...'
COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info ERR3091357 | grep http"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/   VDB_CONFIG=k ${bin_dir}/align-info ERR3091357 | grep http\
                                                                     && exit 597
ls ERR3091357/ERR3091357.sralite ERR3091357/JTFH01 ERR3091357/KN707955.1 \
                                                         > /dev/null || exit 601
echo cd $work_dir
cd ${work_dir}                                                       || exit 603
COMMAND\
="NCBI_SETTINGS=/ VDB_CONFIG=tmp/k $bin_dir/align-info tmp/ERR3091357|grep http"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
NCBI_SETTINGS=/   VDB_CONFIG=tmp/k $bin_dir/align-info tmp/ERR3091357|grep http\
                                                                     && exit 9
rm -r tmp                                                            || exit 569


echo resume:
echo Verifying ${prefetch} resume
rm   -frv tmp/*                                                      || exit 570
mkdir -p  tmp                                                        || exit 571
echo '/repository/site/disabled = "true"' > tmp/k                    || exit 572
COMMAND\
="cd tmp&&PATH=${bin_dir}:${PATH} NCBI_SETTINGS=/ VDB_CONFIG=k perl ../test-resume.pl $VERBOSE"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && PATH=${bin_dir}:${PATH} \
                  NCBI_SETTINGS=/ VDB_CONFIG=k perl ../test-resume.pl $VERBOSE \
                                                            && cd .. || exit 573

rm    -r  tmp                                                        || exit 580


echo hs37d5:
echo Verifying hs37d5
rm   -frv tmp/*                                                      || exit 585
mkdir -p tmp                                                         || exit 586
COMMAND\
="cd tmp&&PATH=$bin_dir:$PATH NCBI_SETTINGS=k perl ../test-hs37d5.pl $VERBOSE"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && PATH=$bin_dir:$PATH NCBI_SETTINGS=k perl ../test-hs37d5.pl $VERBOSE \
                                                                     || exit 591
cd ${work_dir}                                                       || exit 592
rm -r tmp                                                            || exit 593


echo ad_as_dir: # Library test is in
           # ncbi-vdb\test\vfs\managertest.cpp::TestVFSManagerCheckAd
echo we can use AD as dir/...
mkdir -p tmp                                                         || exit 599

COMMAND\
="cd tmp&&PATH=$bin_dir:$PATH NCBI_SETTINGS=/ VDB_CONFIG=k perl ../use-AD-as-dir.pl $VERBOSE"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && PATH=$bin_dir:$PATH NCBI_SETTINGS=/ VDB_CONFIG=k perl ../use-AD-as-dir.pl $VERBOSE\
                                                                     || exit 605
cd ${work_dir}                                                       || exit 606
rm -r tmp                                                            || exit 607


echo dump-vs-prefetch:
rm -frv tmp/*                                                        || exit 611
mkdir -p tmp                                                        || exit 612
COMMAND\
="cd tmp&&PATH=$bin_dir:$PATH perl ../dump-vs-prefetch.pl $VERBOSE"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp && PATH=$bin_dir:$PATH perl ../dump-vs-prefetch.pl $VERBOSE   || exit 616
cd ${work_dir}                                                       || exit 617
rm -r tmp                                                            || exit 618


echo quality:
echo Testing quality
rm -fr tmp                                                           || exit 623
mkdir -p tmp                                                         || exit 624
COMMAND\
="cd tmp&& PATH=$bin_dir:$TESTBINDIR:$PATH NCBI_SETTINGS=/ VDB_CONFIG=k perl ../test-quality.pl $VERBOSE"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
cd tmp  && PATH=$bin_dir:$TESTBINDIR:$PATH NCBI_SETTINGS=/ VDB_CONFIG=k \
                                              perl ../test-quality.pl $VERBOSE \
                                                                     || exit 631
cd ${work_dir}                                                       || exit 632
rm -r tmp                                                            || exit 633
fi # if [ "$NOT_ALL" == "" ]

echo ad_not_cwd:
echo Testing prefetch into output directory and using results
COMMAND\
="PATH=$bin_dir:$PATH perl test-ad-not-cwd.pl $VERBOSE"
if [ "$VERBOSE" != "" ]; then echo $COMMAND; fi
PATH=$bin_dir:$PATH   perl test-ad-not-cwd.pl $VERBOSE               || exit 641
