#!/bin/bash

test_quality=$1
bin_dir=$2

echo test_quality: ${test_quality}
echo cmake_source_dir: ${cmake_source_dir}

echo "testing prefetch"

TEMPDIR=.

echo Testing prefetch from ${bin_dir}

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

#=============================================
# echo urls_and_accs:

# mkdir -p tmp tmp2
# rm -fr tmp/* tmp2/*
# echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg

# echo prefetch URL-1 when there is no kfg
# rm -f wiki
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch https://github.com/ncbi/ngs/wiki)
# rm wiki
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch URL-1 FAILED, res=$res output=$output" && exit 1;
# fi

# echo prefetch URL-2/2 when there is no kfg
# rm -f index.html
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
   # ${bin_dir}/prefetch https://github.com/ncbi/ >/dev/null
# rm index.html
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch URL-2/2, res=$res output=$output" && exit 1;
# fi

# echo prefetch URL-2/1 when there is no kfg
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # if ping -c1 intranet > /dev/null ; then \
       # ${bin_dir}/prefetch http://intranet/ >/dev/null ; rm index.html ; \
    # fi
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch URL-2/1, res=$res output=$output" && exit 1;
# fi

# echo prefetch URL-2/3 when there is no kfg
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # if ping -c1 intranet > /dev/null ; then \
     # ${bin_dir}/prefetch ${HTTP_URL} > /dev/null; \
    # fi
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch URL-2/3, res=$res output=$output" && exit 1;
# fi


# echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg
# echo "${PUBLIC}/apps/file/volumes/flat = \"files\"" >> tmp/t.kfg
# echo "${PUBLIC}/root = \"$(pwd)/tmp\"" >> tmp/t.kfg

# if ls `pwd`/tmp2/${HTTPFILE} 2> /dev/null ; \
 # then echo ${HTTPFILE} found ; exit 1 ; fi
# echo HTTP download when user repository is configured
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
    # if ping -c1 intranet > /dev/null ; then \
    # ${bin_dir}/prefetch ${HTTP_URL} > /dev/null ; fi
# cd ${work_dir} # out of tmp2
# if ping -c1 intranet > /dev/null ; \
 # then ls `pwd`/tmp2/${HTTPFILE} > /dev/null ; fi

# echo Running prefetch second time finds previous download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
 # if ping -c1 intranet > /dev/null ; then \
     # ${bin_dir}/prefetch ${HTTP_URL} \
        # | grep "found local" > /dev/null ; fi
# cd ${work_dir} # out of tmp2

# rm -f ${HTTPFILE}

# if ls `pwd`/tmp2/index.html 2> /dev/null ; \
 # then echo index.shtml found ; exit 1; fi
# echo HTTP download when user repository is configured
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
 # if ping -c1 intranet > /dev/null ; then \
    # ${bin_dir}/prefetch http://intranet/ > /dev/null ; fi
# if ping -c1 intranet>/dev/null; then ls `pwd`/tmp2/index.html > /dev/null;fi
# cd ${work_dir} # out of tmp2

# echo Running prefetch second time finds previous download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
 # if ping -c1 intranet > /dev/null ; then \
    # ${bin_dir}/prefetch http://intranet/ | grep "found local" >/dev/null;fi
# rm -f `pwd`/tmp2/index.html
# cd ${work_dir} # out of tmp2

# echo URL download when user repository is configured
# if ls `pwd`/tmp2/wiki 2> /dev/null ; then echo wiki found ; exit 1; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
    # ${bin_dir}/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
# cd ${work_dir} # out of tmp2
# output=$(ls `pwd`/tmp2/wiki)
# res=$?
# if [ "$res" != "0" ];
	# then echo "wiki FAILED, res=$res output=$output" && exit 1;
# fi

# echo Running prefetch second time finds previous download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
    # ${bin_dir}/prefetch https://github.com/ncbi/ngs/wiki \
                              # | grep "found local" > /dev/null
# cd ${work_dir} # out of tmp2

# echo URL/ download when user repository is configured
# rm -f `pwd`/tmp2/index.html
# if ls `pwd`/tmp2/index.html 2> /dev/null ; \
 # then echo index.html found ; exit 1; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
    # ${bin_dir}/prefetch https://github.com/ncbi/ > /dev/null
# cd ${work_dir} # out of tmp2
# output=$(ls `pwd`/tmp2/index.html)
# res=$?
# if [ "$res" != "0" ];
	# then echo "NANNOT accession FAILED, res=$res output=$output" && exit 1;
# fi

# echo Running prefetch second time finds previous download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/|grep "found local">/dev/null
# cd ${work_dir} # out of tmp2
# rm `pwd`/tmp2/index.html


# if echo ${SRR} | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
  # then \
     # echo prefetch ${SRR} when there is no kfg && \
     # NCBI_VDB_RELIABLE=y NCBI_SETTINGS=/ VDB_CONFIG=`pwd`/tmp \
                               # ${bin_dir}/prefetch ${SRR} > /dev/null && \
     # rm -r ${SRAC} ; \
# else \
     # echo prefetch test when CE is required is skipped ; \
# fi

# echo "${PUBLIC}/apps/sra/volumes/sraFlat = \"sra\"" >> tmp/t.kfg
# rm -f `pwd`/tmp/sra/${SRAC}.sra
# if echo ${SRR} | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
  # then \
	# echo SRR download when user repository is configured && \
	# NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=`pwd`/tmp \
	   # ENV_VAR_LOG_HTTP_RETRY=1 ${bin_dir}/prefetch ${SRR} > /dev/null && \
	# ls `pwd`/tmp/sra/${SRAC}.sra > /dev/null && \
     # \
     # echo Running prefetch second time finds previous download && \
     # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
	# ${bin_dir}/prefetch ${SRR} | grep "found local" > /dev/null ; \
# else \
     # echo prefetch test when CE is required is skipped ; \
# fi

# echo prefetch ${REFSEQ} when there is no kfg
# rm -f KC702174.1
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${REFSEQ} > /dev/null
# rm KC702174.1*

# echo "${PUBLIC}/apps/refseq/volumes/refseq = \"refseq\"" >> tmp/t.kfg
# echo REFSEQ HTTP download when user repository is configured
# if ls `pwd`/tmp/refseq/KC702174.1 2> /dev/null ; \
 # then echo KC702174.1 found ; exit 1; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${REFSEQ} > /dev/null
# output=$(ls `pwd`/tmp/refseq/KC702174.1)
# res=$?
# if [ "$res" != "0" ];
	# then echo "KC702174.1 FAILED, res=$res output=$output" && exit 1;
# fi

# echo Running prefetch second time finds previous download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${REFSEQ} | grep "found local" > /dev/null

# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # if ${bin_dir}/prefetch ${KMER} > /dev/null 2>&1 ; \
    # then echo "prefetch ${KMER} when there is no kfg should fail"; exit 1; \
    # fi

# echo "${PUBLIC}/apps/nakmer/volumes/nakmerFlat = \"nannot\"" >> tmp/t.kfg
# echo NANNOT download when user repository is configured
# if ls `pwd`/tmp/nannot/GCA_000390265.1_R 2> /dev/null ; \
 # then echo GCA_000390265.1_R found ; exit 1 ; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${KMER} > /dev/null
# output=$(ls `pwd`/tmp/nannot/GCA_000390265.1_R)
# res=$?
# if [ "$res" != "0" ];
	# then echo "NANNOT download when user repository is configured FAILED, res=$res output=$output" && exit 1;
# fi

# echo Running prefetch second time finds previous download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${KMER} | grep "found local" > /dev/null

# echo "prefetch ${WGS} when there is no kfg"
# NCBI_SETTINGS=/ VDB_CONFIG=`pwd`/tmp \
       # ${bin_dir}/prefetch ${WGS} > /dev/null 2>&1
# rm AFVF01.1

# echo "${PUBLIC}/apps/wgs/volumes/wgsFlat = \"wgs\"" >> tmp/t.kfg
# echo
# echo WGS HTTP download when user repository is configured
# if ls `pwd`/tmp/wgs/AFVF01.1 2> /dev/null ; \
	# then echo AFVF01.1 found ; exit 1; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${WGS} > /dev/null
# output=$(ls `pwd`/tmp/wgs/AFVF01.1)
# res=$?
# if [ "$res" != "0" ];
	# then echo "WGS HTTP download when user repository is configured FAILED, res=$res output=$output" && exit 1;
# fi
# rm `pwd`/tmp/wgs/AFVF01.1
# echo

# rm -f `pwd`/tmp/refseq/KC702174.1
# if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
    # echo REFSEQ FASP download when user repository is configured ; \
    # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
      # ${bin_dir}/prefetch ${REFSEQF} > /dev/null && \
    # ls `pwd`/tmp/refseq/KC702174.1 > /dev/null ; \
# else echo download of ${REFSEQF} when ascp is not found is disabled ; \
# fi

# rm `pwd`/tmp/nannot/GCA_000390265.1_R
# echo NANNOT FASP download when user repository is configured
# if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
    # echo NANNOT FASP download when user repository is configured ; \
    # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
      # ${bin_dir}/prefetch ${KMERF} > /dev/null ; \
      # ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null ; \
# else echo download of ${KMERF} when ascp is not found is disabled ; \
# fi

# if ls `pwd`/tmp/wgs/AFVF01.1 2> /dev/null ; \
 # then echo AFVF01.1 found ; exit 1; fi
# if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then echo ; \
    # echo WGS FASP download when user repository is configured ; \
    # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
      # ${bin_dir}/prefetch ${WGSF} > /dev/null ; \
    # ls `pwd`/tmp/wgs/AFVF01.1 > /dev/null ; \
    # rm `pwd`/tmp/wgs/AFVF01.1 ; \
# else echo download of ${WGSF} when ascp is not found is disabled ; \
# fi
# echo

# rm -f `pwd`/tmp/sra/${SRAC}.sra

# echo "repository/remote/main/CGI/resolver-cgi = \"${CGI}\"" >> tmp/t.kfg
# echo SRR accession download
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch ${SRAC} > /dev/null
# output=$(ls `pwd`/tmp/sra/${SRAC}.sra)
# res=$?
# if [ "$res" != "0" ];
	# then echo "repository/remote/main/CGI/resolver-cgi = \"${CGI}\" FAILED, res=$res output=$output" && exit 1;
# fi

# echo prefetch run with refseqs
# rm -fr `pwd`/SRR619505
# rm -fv `pwd`/tmp/sra/SRR619505.sra
# rm -fv `pwd`/tmp/refseq/NC_000005.8
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch SRR619505 > /dev/null
# output=$(ls `pwd`/tmp/sra/SRR619505.sra)
# res=$?
# if [ "$res" != "0" ];
	# then echo "SRR619505.sra FAILED, res=$res output=$output" && exit 1;
# fi
# output=$(ls `pwd`/tmp/refseq/NC_000005.8)
# res=$?
# if [ "$res" != "0" ];
	# then echo "NC_000005.8 FAILED, res=$res output=$output" && exit 1;
# fi

# echo REFSEQ accession download
# rm -f `pwd`/tmp/refseq/KC702174.1
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch KC702174.1 > /dev/null
# output=$(ls `pwd`/tmp/refseq/KC702174.1)
# res=$?
# if [ "$res" != "0" ];
	# then echo "REFSEQ accession download FAILED, res=$res output=$output" && exit 1;
# fi

# echo NANNOT accession download
# rm -f `pwd`/tmp/nannot/GCA_000390265.1_R
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch GCA_000390265.1_R > /dev/null
# ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null
# res=$?
# if [ "$res" != "0" ];
	# then echo "NANNOT accession FAILED, res=$res output=$output" && exit 1;
# fi

# echo WGS accession download
# rm -f `pwd`/tmp/wgs/AFVF01
# rm -rf `pwd`/AFVF01
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
    # ${bin_dir}/prefetch AFVF01 -+VFS #> /dev/null
# output=$(ls `pwd`/tmp/wgs/AFVF01)
# res=$?
# if [ "$res" != "0" ];
	# then echo "WGS accession download FAILED, res=$res output=$output" && exit 1;
# fi
# rm `pwd`/tmp/wgs/AFVF01

# if ls `pwd`/tmp2/100MB 2> /dev/null ; \
 # then echo 100MB found ; exit 1; fi
 # echo FASP download: asperasoft/100MB: turned off

# ## TODO DEBUG ascp ?
# if ${bin_dir}/ascp -h > /dev/null ; \
 # then \
    # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
# else \
    # if ${bin_dir}/prefetch ${LARGE} 2> /dev/null ; \
    # then \
        # echo "prefetch <FASP URL> when ascp is not found should fail" ; \
        # exit 1; \
     # fi ; \
# fi
# cd ${work_dir} # out of tmp2

# rm -f `pwd`/tmp2/100MB
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
    # if ${bin_dir}/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:100MB \
                                                         # > /dev/null 2>&1; \
    # then echo "prefetch <FASP> when FASP source is not found should fail"; \
         # exit 1 ; \
    # fi
# cd ${work_dir} # out of tmp2

# rm -r tmp*

# echo vdbcache:
# echo vdbcache download

# rm -frv SRR6667190*

# echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\"" > SRR6667190.kfg

# # download sra and vdbcache
# NCBI_SETTINGS=/ VDB_CONFIG=`pwd` \
# ${bin_dir}/prefetch SRR6667190    > /dev/null
# ls SRR6667190/SRR6667190.sra          > /dev/null
# ls SRR6667190/SRR6667190.sra.vdbcache > /dev/null
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch vdbcache, res=$res output=$output" && exit 1;
# fi
# # TODO: remove test/prefetch/SRR6667190.kfg ?

# # second run of prefetch finds local
# NCBI_SETTINGS=/ VDB_CONFIG=`pwd` \
# ${bin_dir}/prefetch SRR6667190    > /dev/null
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch vdbcache, res=$res output=$output" && exit 1;
# fi

# # download missed sra
# rm SRR6667190/SRR6667190.sra
# NCBI_SETTINGS=/ VDB_CONFIG=`pwd` \
# ${bin_dir}/prefetch SRR6667190    > /dev/null
# ls SRR6667190/SRR6667190.sra          > /dev/null
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch vdbcache, res=$res output=$output" && exit 1;
# fi

# # download missed vdbcache
# rm SRR6667190/SRR6667190.sra.vdbcache
# NCBI_SETTINGS=/ VDB_CONFIG=`pwd` \
# ${bin_dir}/prefetch SRR6667190    > /dev/null
# ls SRR6667190/SRR6667190.sra.vdbcache > /dev/null
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch vdbcache, res=$res output=$output" && exit 1;
# fi

# # prefetch works when AD is empty
# rm SRR6667190/*
# NCBI_SETTINGS=/ VDB_CONFIG=`pwd` \
# ${bin_dir}/prefetch SRR6667190    > /dev/null
# ls SRR6667190/SRR6667190.sra          > /dev/null
# ls SRR6667190/SRR6667190.sra.vdbcache > /dev/null
# res=$?
# if [ "$res" != "0" ];
	# then echo "prefetch vdbcache, res=$res output=$output" && exit 1;
# fi

# # ls SRR6667190
# rm -r SRR6667190*

# echo ncbi1GB:
# mkdir -p tmp

# echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg

# if ls `pwd`/tmp/1GB 2> /dev/null; \
    # then echo 1GB found ; exit 1; fi

 # if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
 # echo FASP download: ncbi/1GB ; \
 # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && cd tmp && \
   # $(DIRTOTEST)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:1GB>/dev/null&&\
   # ls `pwd`/1GB > /dev/null ; \
# fi
# cd ${work_dir}

# rm -r tmp

# ################################################################################

# echo out_dir_and_file:
# mkdir -p tmp tmp2
# rm -fr tmp*/*
# echo  version 1.0     >  tmp/k
# echo '0||SRR053325||' >> tmp/k
# echo '$$end'          >> tmp/k

# echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\"" > tmp/t.kfg
# echo "repository/remote/main/CGI/resolver-cgi = \"${CGI}\"" >> tmp/t.kfg
# echo "${PUBLIC}/apps/sra/volumes/sraFlat = \"sra\""         >> tmp/t.kfg
# echo "${PUBLIC}/root = \"$(pwd)/tmp\""                      >> tmp/t.kfg
# echo "/repository/site/disabled = \"true\""                 >> tmp/t.kfg

# echo PREFETCH ACCESSION TO OUT-FILE
# rm -f `pwd`/tmp-file*
# NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=`pwd`/tmp \
  # ${bin_dir}/prefetch ${SRAC} -o tmp-file -v > /dev/null
# output=$(ls `pwd`/tmp-file)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH ACCESSION TO OUT-FILE FAILED, res=$res output=$output" && exit 1;
# fi
# rm `pwd`/tmp-file

# echo PREFETCH ACCESSION TO OUT-FILE INSIDE OF DIR
# if ls `pwd`/tmp3/dir/file 2> /dev/null ; \
# then echo file found ; exit 1 ; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch ${SRAC} -O / -o tmp3/dir/file -v > /dev/null
# output=$(ls `pwd`/tmp3/dir/file)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH ACCESSION TO OUT-FILE INSIDE OF DIR FAILED, res=$res output=$output" && exit 1;
# fi

# if echo ${SRR} | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
# then \
   # echo PREFETCH SRR HTTP URL TO OUT-FILE && \
   # rm -f `pwd`/tmp3/dir/file && \
   # NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=`pwd`/tmp \
       # ${bin_dir}/prefetch ${SRR} -O / -o tmp3/dir/file > /dev/null && \
   # ls `pwd`/tmp3/dir/file > /dev/null ; \
 # else \
   # echo prefetch test when CE is required is skipped ; \
# fi

# echo PREFETCH SRR FASP URL TO OUT-FILE IS DISABLED
# #	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    # # echo PREFETCH SRR FASP URL TO OUT-FILE ; \
	    # # rm -f `pwd`/tmp3/dir/file ; \
	    # # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    # # ${bin_dir}/prefetch $(SRRF) -O / -o tmp3/dir/file > /dev/null ; \
	    # # ls `pwd`/tmp3/dir/file > /dev/null ; \
	  # # else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  # # fi
# #	@ echo
# #	@ echo

# echo PREFETCH HTTP DIRECTORY URL TO OUT-FILE
# rm -f `pwd`/tmp3/dir/file
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/ -O / -o tmp3/dir/file \
      # > /dev/null
# output=$(ls `pwd`/tmp3/dir/file)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH HTTP DIRECTORY URL TO OUT-FILE FAILED, res=$res output=$output" && exit 1;
# fi


# echo PREFETCH HTTP FILE URL TO OUT-FILE
# rm -f `pwd`/tmp3/dir/file
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/ngs/wiki -O / \
                     # -o tmp3/dir/file > /dev/null
# output=$(ls `pwd`/tmp3/dir/file)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH HTTP FILE URL TO OUT-FILE FAILED, res=$res output=$output" && exit 1;
# fi


# if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
  # echo PREFETCH FASP URL TO OUT-FILE ; \
  # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # rm -f `pwd`/tmp3/dir/file ; \
  # ${bin_dir}/prefetch ${REFSEQF} -O / -o tmp3/dir/file > /dev/null ; \
  # ls `pwd`/tmp3/dir/file > /dev/null ; \
# else echo download of SRR FASP URL when ascp is not found is disabled ; \
# fi

# if [ "${BUILD}" = "dbg" ]; then \
  # echo PREFETCH TEXT-KART was disabled: need to have a public example of protected run or add support of public accessions; \
  # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # echo ${bin_dir}/prefetch --text-kart tmp/k > /dev/null ; \
  # echo ls `pwd`/tmp/sra/SRR053325.sra > /dev/null ; \
# fi

# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
# if ${bin_dir}/prefetch SRR045450 ${SRAC} -o tmp/o 2> /dev/null ; \
# then echo unexpected success when downloading multiple items to file ; \
    # exit 1 ; \
# fi ;

# echo PREFETCH MULTIPLE ITEMS
# rm -fr `pwd`/SRR0* `pwd`/tmp/sra
# ls `pwd`/tmp
# cat `pwd`/tmp/t.kfg
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch SRR045450 ${SRAC} -+VFS #> /dev/null
# output=$(ls `pwd`/tmp/sra/SRR045450.sra `pwd`/tmp/sra/${SRAC}.sra)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH MULTIPLE ITEMS FAILED, res=$res output=$output" && exit 1;
# fi


# if echo ${SRR} | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
# then \
   # echo PREFETCH SRR HTTP URL && \
   # rm `pwd`/tmp/sra/${SRAC}.sra && \
   # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
                             # ${bin_dir}/prefetch ${SRR} > /dev/null && \
   # ls `pwd`/tmp/sra/${SRAC}.sra > /dev/null ; \
# else \
   # echo prefetch test when CE is required is skipped ; \
# fi

# echo PREFETCH SRR FASP URL IS DISABLED
# #	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    # # echo PREFETCH SRR FASP URL ; \
	    # # rm `pwd`/tmp/sra/${SRAC}.sra ; \
	    # # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    # # ${bin_dir}/prefetch $(SRRF) > /dev/null ; \
	    # # ls `pwd`/tmp/sra/${SRAC}.sra > /dev/null ; \
	  # # else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  # # fi

# echo PREFETCH HTTP DIRECTORY URL
# if ls `pwd`/tmp2/index.html 2> /dev/null ; then exit 1; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/ -vv #> /dev/null
# cd ${work_dir}
# echo `pwd`
# output=$(ls `pwd`/tmp2/index.html)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH HTTP DIRECTORY URL FAILED, res=$res output=$output" && exit 1;
# fi
# rm `pwd`/tmp2/index.html

# echo PREFETCH HTTP FILE URL
# if ls `pwd`/tmp2/wiki 2> /dev/null ; \
# then echo wiki found ; exit 1 ; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
# cd ${work_dir}
# output=$(ls `pwd`/tmp2/wiki)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH HTTP FILE URL FAILED, res=$res output=$output" && exit 1;
# fi

# if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
  # echo PREFETCH FASP URL ; \
  # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
  # if ls `pwd`/KC702174.1 2> /dev/null ; then exit 1; fi ; \
  # ${bin_dir}/prefetch ${REFSEQF} > /dev/null && \
  # ls `pwd`/KC702174.1 > /dev/null ; \
# else echo download of FASP URL when ascp is not found is disabled ; \
# fi
# cd ${work_dir} # TODO if we need to use ${work_dir} istead of pwd a few lines above

# echo PREFETCH ACCESSION TO OUT-DIR
# rm -f `pwd`/tmp3/dir/${SRAC}/${SRAC}.sra
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch ${SRAC} -O tmp3/dir > /dev/null
# output=$(ls `pwd`/tmp3/dir/${SRAC}/${SRAC}.sra)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH ACCESSION TO OUT-DIR FAILED, res=$res output=$output" && exit 1;
# fi


# if echo ${SRR} | grep -vq /sdlr/sdlr.fcgi?jwt= ; \
# then \
   # echo PREFETCH SRR HTTP URL TO OUT-DIR && \
   # rm -f `pwd`/tmp3/dir/${SRAC}/${SRAC}.sra && \
   # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
                 # ${bin_dir}/prefetch ${SRR} -O tmp3/dir > /dev/null && \
   # ls `pwd`/tmp3/dir/${SRAC}/${SRAC}.sra > /dev/null ; \
# else \
   # echo prefetch test when CE is required is skipped ; \
# fi

# echo PREFETCH SRR FASP URL TO OUT-DIR IS DISABLED
# #	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    # # echo PREFETCH SRR FASP URL TO OUT-DIR ; \
	    # # rm -f `pwd`/tmp3/dir/${SRAC}.sra ; \
	    # # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    # # ${bin_dir}/prefetch $(SRRF) -O tmp3/dir > /dev/null ; \
	    # # ls `pwd`/tmp3/dir/${SRAC}.sra > /dev/null ; \
	  # # else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  # # fi
# #	@ echo

# echo PREFETCH HTTP DIRECTORY URL TO OUT-DIR
# rm -f index.html
# if ls `pwd`/tmp3/dir/index.html 2> /dev/null ; \
# then echo index.html found ; exit 1 ; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/ -O tmp3/dir > /dev/null
# output=$(ls `pwd`/tmp3/dir/index.html)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH HTTP DIRECTORY URL TO OUT-DIR FAILED, res=$res output=$output" && exit 1;
# fi
# echo

# echo PREFETCH HTTP FILE URL TO OUT-DIR
# rm -f wiki
# if ls `pwd`/tmp3/dir/wiki 2> /dev/null ; \
# then echo wiki found ; exit 1 ; fi
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch https://github.com/ncbi/ngs/wiki -O tmp3/dir \
                     # > /dev/null
# output=$(ls `pwd`/tmp3/dir/wiki)
# res=$?
# if [ "$res" != "0" ];
	# then echo "PREFETCH HTTP FILE URL TO OUT-DIR FAILED, res=$res output=$output" && exit 1;
# fi
# #	@ echo

# if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
  # echo PREFETCH FASP URL TO OUT-DIR ; \
  # export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # if ls `pwd`/tmp3/dir/KC702174.1 2> /dev/null ; then \
  # echo 100MB found ; exit 1 ; fi ; \
  # ${bin_dir}/prefetch ${REFSEQF} -O tmp3/dir > /dev/null && \
  # ls `pwd`/tmp3/dir/KC702174.1 > /dev/null ; \
# else echo download of SRR FASP URL when ascp is not found is disabled ; \
# fi
# #	@ echo

# rm -r tmp*

# echo s-option:
# mkdir -p tmp
# echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg
# echo 'repository/remote/main/CGI/resolver-cgi = "${CGI}"' >> tmp/t.kfg
# echo '${PUBLIC}/apps/sra/volumes/sraFlat = "sra"'         >> tmp/t.kfg
# echo '${PUBLIC}/root = "$(pwd)/tmp"'                      >> tmp/t.kfg

# echo Downloading KART was disabled: need to have a public example of protected run or add support of public accessions
# rm -f tmp/sra/SRR0*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/prefetch kart.krt > /dev/null

# echo Downloading KART: ORDER BY SIZE / SHORT OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/prefetch kart.krt -os > /dev/null

# echo Downloading KART: ORDER BY KART / SHORT OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/prefetch kart.krt -ok > /dev/null

# echo Downloading KART: ORDER BY SIZE / LONG OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/prefetch kart.krt --order s > /dev/null

# echo Downloading KART: ORDER BY KART / LONG OPTION disabled, too
# rm -f tmp/sra/*
# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	# #	${bin_dir}/prefetch kart.krt --order k > /dev/null

# echo Downloading ACCESSION
# rm -f tmp/sra/${SRAC}.sra
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch ${SRAC})
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSION FAILED, res=$res output=$output" && exit 1;
# fi

# echo Downloading ACCESSION TO FILE: SHORT OPTION
# rm -f tmp/1
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch ${SRAC} -otmp/1)
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSION TO FILE: SHORT OPTION FAILED, res=$res output=$output" && exit 1;
# fi
# #	@ echo

# echo Downloading ACCESSION TO FILE: LONG OPTION
# rm -f tmp/2
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch ${SRAC} --output-file tmp/2)
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSION TO FILE: LONG OPTION FAILED, res=$res output=$output" && exit 1;
# fi

# echo Downloading ACCESSIONS
# rm -f tmp/sra/*
# output=$(export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
  # ${bin_dir}/prefetch ${SRAC} SRR045450)
# res=$?
# if [ "$res" != "0" ];
	# then echo "Downloading ACCESSIONS FAILED, res=$res output=$output" && exit 1;
# fi

# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
# if ${bin_dir}/prefetch SRR045450 ${SRAC} -o tmp/o 2> /dev/null ; \
# then echo unexpected success when downloading multiple items to file ; \
    # exit 1 ; \
# fi ;

# export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
# if ${bin_dir}/prefetch SRR045450 ${SRAC} -output-file tmp/o \
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
      # ${bin_dir}/prefetch --text-kart tmp/k > /dev/null && \
      # \
      # echo Downloading TEXT KART: ORDER BY SIZE / SHORT OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/prefetch --text-kart tmp/k -os > /dev/null 2>&1 && \
      # \
      # echo Downloading TEXT KART: ORDER BY KART / SHORT OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/prefetch --text-kart tmp/k -ok > /dev/null 2>&1 && \
      # \
      # echo Downloading TEXT KART: ORDER BY SIZE / LONG OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/prefetch --text-kart tmp/k --order s > /dev/null && \
      # \
      # echo Downloading TEXT KART: ORDER BY KART / LONG OPTION && \
      # rm -f tmp/sra/* && \
      # export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
      # ${bin_dir}/prefetch --text-kart tmp/k --order k > /dev/null ; \
  # fi \
  # fi

# rm -r tmp*

echo truncated:
	echo prefetch correctly re-downloads incomplete files

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
VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/prefetch ${SRAC} 2>tmp/out \
    || exit 861
 if grep ": 1) '${SRAC}' is found locally" tmp/out ; \
 then echo "prefetch incorrectly stopped on truncated run"; exit 1; \
 fi
#	@ wc tmp/sra/${SRAC}.sra	@ wc -c tmp/sra/${SRAC}.sra

# run should be complete
perl check-size.pl 4 || exit 868

# the second run of prefetch should find local file
VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ ${bin_dir}/prefetch ${SRAC} 2>tmp/out \
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
 "${bin_dir}/prefetch -+VFS --ngc ../data/prj_phs710EA_test.ngc --cart ../data/3-dbGaP-0.krt -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -Cn" \
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -os -Cn"
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
 "${bin_dir}/prefetch --ngc ../data/prj_phs710EA_test.ngc ../data/3-dbGaP-0.krt -ok -Cn" \
                                                                     || exit 1009
cd ${work_dir}                                                       || exit 1010
rm tmp/SRR1219879/SRR1219879_dbGaP-0.sra*                            || exit 1011
rm tmp/SRR1219880/SRR1219880_dbGaP-0.sra*                            || exit 1012
rm tmp/SRR1257493/SRR1257493_dbGaP-0.sra*                            || exit 1013
rmdir tmp/SRR1219879 tmp/SRR1219880 tmp/SRR1257493                   || exit 1014

rm -r tmp                                                            || exit 1015

echo wgs:
echo Verifying prefetch of runs with WGS references...

rm -fr tmp                                                           || exit 1021
mkdir  tmp                                                           || exit 1022

#pwd
echo "/LIBS/GUID = \"8test002-6ab7-41b2-bfd0-prefetchpref\"" > tmp/k || exit 1025

cd tmp && NCBI_SETTINGS=k    ${bin_dir}/prefetch SRR619505 -fy > /dev/null 2>&1 \
                                                                     || exit 1028
cd ${work_dir}                                                       || exit 1029
ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null || exit 1030
rm -r tmp/S*                                                         || exit 1031

echo '/libs/vdb/quality = "ZR"'                           >> tmp/k   || exit 1033
echo '/repository/site/disabled = "true"'                 >> tmp/k   || exit 1034
cd tmp && NCBI_SETTINGS=k   ${bin_dir}/prefetch SRR619505  -fy > /dev/null 2>&1 \
                                                                     || exit 1036
cd ${work_dir}                                                       || exit 1037
ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null || exit 1016
rm -r tmp/[NS]*                                                      || exit 1039

echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"'                 >> tmp/k  || exit 1041
echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
	                                                       >> tmp/k  || exit 1043
echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k \
                                                                     || exit 1045
printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd` >> tmp/k||exit 1046
#cat tmp/k

cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/prefetch SRR619505  -fy \
                                                    > /dev/null 2>&1 || exit 1050
cd ${work_dir}                                                       || exit 1051
ls tmp/SRR619505/SRR619505.sra tmp/refseq/NC_000005.8    > /dev/null || exit 1052
rm -r tmp/[rS]*                                                      || exit 1053

#TODO: TO ADD AD for AAAB01 cd tmp &&                 prefetch SRR353827 -fy -+VFS

cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/prefetch SRR353827  -fy \
                                                    > /dev/null 2>&1 || exit 1058
cd ${work_dir}                                                       || exit 1059
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info SRR353827 \
	| grep -v 'false,remote::https:'                     > /dev/null || exit 1061
cd ${work_dir}                                                       || exit 1062
ls tmp/SRR353827/SRR353827.sra tmp/wgs/AAAB01            > /dev/null || exit 1063
rm -r tmp                                                            || exit 1064

echo lots_wgs:
echo  Verifying prefetch of run with with a lot of WGS references...
rm -fr tmp                                                           || exit 1068
mkdir  tmp                                                           || exit 1069

#pwd
echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"'   > tmp/k || exit 1072
echo '/repository/site/disabled = "true"'                   >> tmp/k || exit 1073
echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"'                  >> tmp/k || exit 1074
echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
	                                                        >> tmp/k || exit 1076
echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k \
                                                                     || exit 1078
printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd`        >> tmp/k \
                                                                     || exit 1080
#cat tmp/k

echo '      ... prefetching...'
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/prefetch   ERR3091357 -fy \
                                                   > /dev/null  2>&1 || exit 1085
cd ${work_dir}                                                       || exit 1086
echo '      ... align-infoing...'
cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k ${bin_dir}/align-info ERR3091357 \
	| grep -v 'false,remote::https:'                     > /dev/null || exit 1089
cd ${work_dir}                                                       || exit 1090
ls tmp/ERR3091357/ERR3091357.sra tmp/wgs/JTFH01 tmp/refseq/KN707955.1 \
                                                         > /dev/null || exit 1092

rm -r tmp                                                            || exit 1094

echo resume:
echo Verifying prefetch resume
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
#	@ cd tmp && NCBI_SETTINGS=k PATH='${bin_dir}:$(BINDIR):${PATH}' \
				perl ../test-quality.pl
cd ${work_dir}                                                       || exit 1138
rm -r tmp                                                            || exit 1139

echo ad_not_cwd:
echo Testing prefetch into output directory and using results
PATH="${bin_dir}:${PATH}" perl test-ad-not-cwd.pl                    || exit 1143
