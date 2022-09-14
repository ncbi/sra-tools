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
# ===========================================================================

test_quality=$1
bin_dir=$2
prefetch=$3

echo test_quality: ${test_quality}
echo cmake_source_dir: ${cmake_source_dir}

echo "testing ${prefetch}"

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
CGI=https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi
SDL=https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve
LARGE=fasp://aspera@demo.asperasoft.com:aspera-test-dir-large/100MB
SRA=https://sra-download.ncbi.nlm.nih.gov
SRAF=fasp://dbtest@sra-download.ncbi.nlm.nih.gov
#SRAF=fasp://dbtest@srafiles31-1.st-va.ncbi.nlm.nih.gov
KMER=${SRA}/traces/nannot01/kmer/000/390/GCA_000390265.1_R
KMERF=${SRAF}:data/sracloud/traces/nannot01/kmer/000/390/GCA_000390265.1_R
REFSEQ=${SRA}/traces/refseq/KC702174.1
REFSEQF=${SRAF}:data/sracloud/traces/refseq/KC702174.1
SRAC=SRR053325
SRR=${SRA}/sos2/sra-pub-run-15/${SRAC}/${SRAC}.4
SRR=$(NCBI_SETTINGS=/ ${bin_dir}/srapath ${SRAC})
INT=https://sra-download-internal.ncbi.nlm.nih.gov
DDB=https://sra-downloadb.be-md.ncbi.nlm.nih.gov
#SRR=${DDB}/sos/sra-pub-run-5/${SRAC}/${SRAC}.3
SRAI=fasp://dbtest@sra-download-internal.ncbi.nlm.nih.gov
SRRF=${SRAF}:data/sracloud/traces/sra57/SRR/000052/${SRAC}
SRRF=${SRAI}:data/sracloud/traces/sra57/SRR/000052/${SRAC}
WGS=${SRA}/traces/wgs03/WGS/AF/VF/AFVF01.1
WGSF=${SRAF}:data/sracloud/traces/wgs03/WGS/AF/VF/AFVF01.1

work_dir=$(pwd)
echo WORK DIRECTORY: ${work_dir}

# ##############################################################################
# echo s-option:
# mkdir -p tmp
# echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg
# echo 'repository/remote/main/CGI/resolver-cgi = "${CGI}"' >> tmp/t.kfg
# echo '${PUBLIC}/apps/sra/volumes/sraFlat = "sra"'         >> tmp/t.kfg
# echo '${PUBLIC}/root = "$(pwd)/tmp"'                      >> tmp/t.kfg

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

# echo Downloading ACCESSION
# rm -f tmp/sra/${SRAC}.sra
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/${prefetch} ${SRAC})
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSION FAILED, res=$res output=$output" && exit 1;
# fi

# echo Downloading ACCESSION TO FILE: SHORT OPTION
# rm -f tmp/1
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/${prefetch} ${SRAC} -otmp/1)
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSION TO FILE: SHORT OPTION FAILED, res=$res output=$output" && exit 1;
# fi
# #	@ echo

# echo Downloading ACCESSION TO FILE: LONG OPTION
# rm -f tmp/2
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/${prefetch} ${SRAC} --output-file tmp/2)
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSION TO FILE: LONG OPTION FAILED, res=$res output=$output" && exit 1;
# fi

# echo Downloading ACCESSIONS
# rm -f tmp/sra/*
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/${prefetch} ${SRAC} SRR045450)
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSIONS FAILED, res=$res output=$output" && exit 1;
# fi

# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
# if ${bin_dir}/${prefetch} SRR045450 ${SRAC} -o tmp/o 2> /dev/null ; \
# then echo unexpected success when downloading multiple items to file ; \
    # exit 1 ; \
# fi ;

# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
# if ${bin_dir}/${prefetch} SRR045450 ${SRAC} -output-file tmp/o \
                                                          # 2> /dev/null ; \
# then echo unexpected success when downloading multiple items to file ; \
    # exit 1 ; \
# fi ;

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

# rm -r tmp*

echo truncated:
	echo ${prefetch} correctly re-downloads incomplete files

#setup
mkdir -p tmp/sra || exit 839
echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\"" > tmp/t.kfg \
    || exit 841
echo "repository/remote/main/CGI/resolver-cgi = \"${CGI}\"" >> tmp/t.kfg \
    || exit 843
echo "${PUBLIC}/apps/sra/volumes/sraFlat = \"sra\""         >> tmp/t.kfg \
    || exit 844
echo "${PUBLIC}/root = \"$(pwd)/tmp\""                      >> tmp/t.kfg \
    || exit 847
#	@ ls -l tmp	@ cat tmp/t.kfg
#	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/vdb-config -on
#	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/srapath ${SRAC}
#	echo 669	perl check-size.pl 4	echo 671

# create truncated run
echo '123' > tmp/sra/${SRAC}.sra || exit 854
if perl check-size.pl 4 > /dev/null; \
then echo "run should be truncated for this test"; exit 1; \
fi

# prefetch should detect truncated run and redownload it
VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/${prefetch} ${SRAC} 2>tmp/out \
    || exit 861
 if grep ": 1) '${SRAC}' is found locally" tmp/out ; \
 then echo "${prefetch} incorrectly stopped on truncated run"; exit 1; \
 fi
#	@ wc tmp/sra/${SRAC}.sra	@ wc -c tmp/sra/${SRAC}.sra

# run should be complete
perl check-size.pl 4 || exit 868

# the second run of prefetch should find local file
VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/${prefetch} ${SRAC} 2>tmp/out \
                                                   || exit 872
 grep -q ": 1) '${SRAC}' is found locally" tmp/out || exit 873
 perl check-size.pl 4                              || exit 874

#cleanup
rm -r tmp                                          || exit 877

echo kart: ##########################################################################
rm -frv tmp/* || exit 880
mkdir -p tmp  || exit 881
echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\""   > tmp/t.kfg \
    || exit 883
echo "repository/remote/main/SDL.2/resolver-cgi = \"${SDL}\"" >> tmp/t.kfg \
    || exit 885

echo "Downloading --cart <kart> ordered by default (size)"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc --cart ../data/3-dbGaP-0.krt -Cn" \
                                                    || exit 892
cd ${work_dir}                                      || exit 893
rm     tmp/SRR1219879/SRR1219879_dbGaP-0.sra*       || exit 894
rm     tmp/SRR1219880/SRR1219880_dbGaP-0.sra*       || exit 895
rm     tmp/SRR1257493/SRR1257493_dbGaP-0.sra*       || exit 896
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 897

echo "Downloading kart ordered by default (size)"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
                                                    || exit 904
cd ${work_dir}                                      || exit 905
rm     tmp/SRR1219879/SRR1219879_dbGaP-0.sra*       || exit 906
rm     tmp/SRR1219880/SRR1219880_dbGaP-0.sra*       || exit 907
rm     tmp/SRR1257493/SRR1257493_dbGaP-0.sra*       || exit 908
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 909

echo "Downloading kart ordered by size"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn" \
                                                    || exit 916
cd ${work_dir}                                      || exit 917
rm     tmp/SRR1219879/SRR1219879_dbGaP-0.sra*       || exit 918
rm     tmp/SRR1219880/SRR1219880_dbGaP-0.sra*       || exit 919
rm     tmp/SRR1257493/SRR1257493_dbGaP-0.sra*       || exit 920
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 921

echo "Downloading kart ordered by kart"
cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                                    || exit 928
cd ${work_dir}                                      || exit 929
rm tmp/SRR1219879/SRR1219879_dbGaP-0.sra*           || exit 930
rm tmp/SRR1219880/SRR1219880_dbGaP-0.sra*           || exit 931
rm tmp/SRR1257493/SRR1257493_dbGaP-0.sra*           || exit 932
rmdir tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493  || exit 933

rm -frv tmp/sra                                     || exit 935
echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\""   > tmp/t.kfg \
    || exit 937
echo "repository/remote/main/SDL.2/resolver-cgi = \"${SDL}\"" >> tmp/t.kfg \
    || exit 939
echo "${PUBLIC}/apps/sra/volumes/sraFlat = \"sra\"" >> tmp/t.kfg || exit 940
echo "${PUBLIC}/root = \"$(pwd)/tmp\""              >> tmp/t.kfg || exit 941

echo "Downloading kart ordered by default (size) to user repo"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
                                      || exit 948
cd ${work_dir}                        || exit 949
rm    tmp/sra/SRR1219879_dbGaP-0.sra* || exit 950
rm    tmp/sra/SRR1219880_dbGaP-0.sra* || exit 951
rm    tmp/sra/SRR1257493_dbGaP-0.sra* || exit 952
rmdir tmp/sra                         || exit 953

echo "Downloading kart ordered by size to user repo"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn" \
                                      || exit 960
cd ${work_dir}                        || exit 961
rm    tmp/sra/SRR1219879_dbGaP-0.sra* || exit 962
rm    tmp/sra/SRR1219880_dbGaP-0.sra* || exit 963
rm    tmp/sra/SRR1257493_dbGaP-0.sra* || exit 964
rmdir tmp/sra                         || exit 965

echo "Downloading kart ordered by kart to user repo"
cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                      || exit 972
cd ${work_dir}                        || exit 973
rm    tmp/sra/SRR1219879_dbGaP-0.sra* || exit 974
rm    tmp/sra/SRR1219880_dbGaP-0.sra* || exit 975
rm    tmp/sra/SRR1257493_dbGaP-0.sra* || exit 976
rmdir tmp/sra                         || exit 977

echo '/tools/prefetch/download_to_cache = "false"' >> tmp/t.kfg || exit 979
echo "Downloading kart ordered by default (size) ignoring user repo"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
                                                    || exit 985
cd ${work_dir}                                      || exit 986
rm     tmp/SRR1219879/SRR1219879_dbGaP-0.sra*       || exit 987
rm     tmp/SRR1219880/SRR1219880_dbGaP-0.sra*       || exit 988
rm     tmp/SRR1257493/SRR1257493_dbGaP-0.sra*       || exit 989
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 990

echo "Downloading kart ordered by size ignoring user repo"
cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn"
cd ${work_dir} \
                                                    || exit  998
rm     tmp/SRR1219879/SRR1219879_dbGaP-0.sra*       || exit  999
rm     tmp/SRR1219880/SRR1219880_dbGaP-0.sra*       || exit 1000
rm     tmp/SRR1257493/SRR1257493_dbGaP-0.sra*       || exit 1001
rmdir  tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493 || exit 1002

echo "Downloading kart ordered by kart ignoring user repo"
cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
NCBI_SETTINGS=/ VDB_CONFIG=. \
perl ../check-prefetch-out.pl 0 0 \
 "${bin_dir}/${prefetch} --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                                                     || exit 1009
cd ${work_dir}                                                       || exit 1010
rm tmp/SRR1219879/SRR1219879_dbGaP-0.sra*                            || exit 1011
rm tmp/SRR1219880/SRR1219880_dbGaP-0.sra*                            || exit 1012
rm tmp/SRR1257493/SRR1257493_dbGaP-0.sra*                            || exit 1013
rmdir tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493                   || exit 1014

rm -r tmp                                                            || exit 1015

echo wgs:
echo Verifying ${prefetch} of runs with WGS references...

rm -fr tmp                                                           || exit 1021
mkdir  tmp                                                           || exit 1022

#pwd
echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\"" > tmp/k || exit 1025

cd tmp && NCBI_SETTINGS=k    ${bin_dir}/${prefetch} SRR619505 -fy > /dev/null 2>&1 \
                                                                     || exit 1028
cd ${work_dir}                                                       || exit 1029
ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null || exit 1030
rm -r tmp/S*                                                         || exit 1031

echo '/libs/vdb/quality = "ZR"'                           >> tmp/k   || exit 1033
echo '/repository/site/disabled = "true"'                 >> tmp/k   || exit 1034
cd tmp && NCBI_SETTINGS=k   ${bin_dir}/${prefetch} SRR619505  -fy > /dev/null 2>&1 \
                                                                     || exit 1036
cd ${work_dir}                                                       || exit 1037
ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null || exit 1016
rm -r tmp/[NS]*                                                      || exit 1039

echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"' >> tmp/k  || exit 1041
echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
	                                                       >> tmp/k  || exit 1043
echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k \
                                                                     || exit 1045
printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd` >> tmp/k||exit 1046
#cat tmp/k

cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/${prefetch} SRR619505  -fy \
                                                    > /dev/null 2>&1 || exit 1050
cd ${work_dir}                                                       || exit 1051
ls tmp/SRR619505/SRR619505.sra tmp/refseq/NC_000005.8    > /dev/null || exit 1052
rm -r tmp/[rS]*                                                      || exit 1053

#TODO: TO ADD AD for AAAB01 cd tmp &&                 prefetch SRR353827 -fy -+VFS

cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/${prefetch} SRR353827  -fy \
                                                    > /dev/null 2>&1 || exit 1058
cd ${work_dir}                                                       || exit 1059
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info SRR353827 \
	| grep -v 'false,remote::https:'                     > /dev/null || exit 1061
cd ${work_dir}                                                       || exit 1062
ls tmp/SRR353827/SRR353827.sralite tmp/wgs/AAAB01            > /dev/null || exit 1063
rm -r tmp                                                            || exit 1064

echo lots_wgs:
echo  Verifying ${prefetch} of run with with a lot of WGS references...
rm -fr tmp                                                           || exit 1068
mkdir  tmp                                                           || exit 1069

#pwd
echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"'   > tmp/k || exit 1072
echo '/repository/site/disabled = "true"'                   >> tmp/k || exit 1073
echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"' >> tmp/k || exit 1074
echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
	                                                        >> tmp/k || exit 1076
echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k \
                                                                     || exit 1078
printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd`        >> tmp/k \
                                                                     || exit 1080
#cat tmp/k

echo '      ... prefetching...'
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/${prefetch}   ERR3091357 -fy \
                                                   > /dev/null  2>&1 || exit 1085
cd ${work_dir}                                                       || exit 1086
echo '      ... align-infoing...'
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info ERR3091357 \
	| grep -v 'false,remote::https:'                     > /dev/null || exit 1089
cd ${work_dir}                                                       || exit 1090
ls tmp/ERR3091357/ERR3091357.sralite tmp/wgs/JTFH01 tmp/refseq/KN707955.1 \
                                                         > /dev/null || exit 1092

rm -r tmp                                                            || exit 1094

echo resume:
echo Verifying ${prefetch} resume
rm   -frv tmp/*                                                      || exit 1098
mkdir -p  tmp                                                        || exit 1099
echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/k   || exit 1100
echo '/repository/site/disabled = "true"'                 >> tmp/k   || exit 1101
cd tmp && PATH="${bin_dir}:${PATH}" NCBI_SETTINGS=k perl ../test-resume.pl \
                                                            && cd .. || exit 1103
rm    -r  tmp                                                        || exit 1104

echo hs37d5:
echo Verifying hs37d5
rm   -frv tmp/*                                                      || exit 1108
mkdir -p  tmp                                                        || exit 1109
cd tmp && NCBI_SETTINGS=k PATH="${bin_dir}:${PATH}" perl ../test-hs37d5.pl \
                                                                     || exit 1111
cd ${work_dir}                                                       || exit 1112
rm    -r  tmp                                                        || exit 1113

echo ad_as_dir: # turned off by default. Library test is in
           # ncbi-vdb\test\vfs\managertest.cpp::TestVFSManagerCheckAd
echo we can use AD as dir/...
mkdir -p tmp                                                         || exit 1118
export NCBI_SETTINGS=/ && export PATH="${bin_dir}:${PATH}" \
	&& cd tmp && perl ../use-AD-as-dir.pl                            || exit 1120
cd ${work_dir}                                                       || exit 1121
rm -r tmp                                                            || exit 1122

echo dump-vs-prefetch:
rm -frv tmp/*                                                        || exit 1125
mkdir -pv tmp                                                        || exit 1126
cd tmp && NCBI_SETTINGS=/ NCBI_VDB_NO_ETC_NCBI_KFG=1 PATH="${bin_dir}:${PATH}" \
                               perl ../dump-vs-prefetch.pl           || exit 1128
cd ${work_dir}                                                       || exit 1129
rm -r tmp                                                            || exit 1130

echo quality: std
echo Testing quality
mkdir -p tmp                                                         || exit 1134
echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/k   || exit 1135
#	@ cd tmp && NCBI_SETTINGS=k PATH='${bin_dir}:$(BINDIR):${PATH}' perl test-quality.pl

echo ad_not_cwd:
echo Testing prefetch into output directory and using results
cd tmp && NCBI_SETTINGS=k PATH="${bin_dir}:${PATH}" perl ../test-ad-not-cwd.pl || exit 1143

cd ${work_dir}                                                       || exit 1138
rm -r tmp                                                            || exit 1139
