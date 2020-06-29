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

default: runtests
TOP ?= $(abspath ../..)
include $(TOP)/build/Makefile.env

CWD = $(shell pwd)
PUBLIC=/repository/user/main/public
CGI=https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi
SDL=https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve
LARGE=fasp://aspera@demo.asperasoft.com:aspera-test-dir-large/100MB
SRA=https://sra-download.ncbi.nlm.nih.gov
SRAF=fasp://dbtest@sra-download.ncbi.nlm.nih.gov
#SRAF=fasp://dbtest@srafiles31-1.st-va.ncbi.nlm.nih.gov
KMER=$(SRA)/traces/nannot01/kmer/000/390/GCA_000390265.1_R
KMERF=$(SRAF):data/sracloud/traces/nannot01/kmer/000/390/GCA_000390265.1_R
REFSEQ=$(SRA)/traces/refseq/KC702174.1
REFSEQF=$(SRAF):data/sracloud/traces/refseq/KC702174.1
SRAC=SRR053325
SRR=$(SRA)/sos2/sra-pub-run-15/$(SRAC)/$(SRAC).4
SRR=$(shell ${BINDIR}/srapath ${SRAC})
INT=https://sra-download-internal.ncbi.nlm.nih.gov
DDB=https://sra-downloadb.be-md.ncbi.nlm.nih.gov
#SRR=$(DDB)/sos/sra-pub-run-5/$(SRAC)/$(SRAC).3
SRAI=fasp://dbtest@sra-download-internal.ncbi.nlm.nih.gov
SRRF=$(SRAF):data/sracloud/traces/sra57/SRR/000052/$(SRAC)
SRRF=$(SRAI):data/sracloud/traces/sra57/SRR/000052/$(SRAC)
WGS=$(SRA)/traces/wgs03/WGS/AF/VF/AFVF01.1
WGSF=$(SRAF):data/sracloud/traces/wgs03/WGS/AF/VF/AFVF01.1
B=$(BINDIR)

HTTP_URL=https://test.ncbi.nlm.nih.gov/home/about/contact.shtml
HTTPFILE=contact.shtml

runtests: urls_and_accs out_dir_and_file s-option truncated kart wgs resume
slowtests: hs37d5 ncbi1GB

################################################################################
urls_and_accs:
	@mkdir -p tmp tmp2
	@rm -fr tmp/* tmp2/*
	@echo 'foo = "bar"' > tmp/t.kfg

	@echo prefetch URL-1 when there is no kfg
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null

	@echo prefetch URL-2/2 when there is no kfg
	@rm -f index.html
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ >/dev/null

	@echo prefetch URL-2/1 when there is no kfg
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if ping -c1 intranet > /dev/null ; then \
	       $(BINDIR)/prefetch http://intranet/ >/dev/null ; \
	    fi

	@echo prefetch URL-2/3 when there is no kfg
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if ping -c1 intranet > /dev/null ; then \
	     $(BINDIR)/prefetch $(HTTP_URL)  > /dev/null; \
	    fi

	@echo '$(PUBLIC)/apps/file/volumes/flat = "files"' > tmp/t.kfg
	@echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg

	@if ls `pwd`/tmp2/$(HTTPFILE) 2> /dev/null ; \
     then echo $(HTTPFILE) found ; exit 1 ; fi
	@echo HTTP download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if ping -c1 intranet > /dev/null ; then \
	    $(BINDIR)/prefetch $(HTTP_URL) > /dev/null ; fi
	@if ping -c1 intranet > /dev/null ; \
	 then ls `pwd`/tmp2/$(HTTPFILE) > /dev/null ; fi

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 if ping -c1 intranet > /dev/null ; then \
	     $(BINDIR)/prefetch $(HTTP_URL) \
	        | grep "found local" > /dev/null ; fi

	@rm -f $(HTTPFILE)

	@if ls `pwd`/tmp2/index.html 2> /dev/null ; \
     then echo index.shtml found ; exit 1; fi
	@echo HTTP download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 if ping -c1 intranet > /dev/null ; then \
	    $(BINDIR)/prefetch http://intranet/ > /dev/null ; fi
	@if ping -c1 intranet>/dev/null; then ls `pwd`/tmp2/index.html > /dev/null;fi

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 if ping -c1 intranet > /dev/null ; then \
	    $(BINDIR)/prefetch http://intranet/ | grep "found local" > /dev/null ; fi
	@rm -f tmp2/index.html

	@echo URL download when user repository is configured
	@if ls `pwd`/tmp2/wiki 2> /dev/null ; then echo wiki found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@ls `pwd`/tmp2/wiki > /dev/null

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki \
	                              | grep "found local" > /dev/null

	@echo URL/ download when user repository is configured
	@if ls `pwd`/tmp2/index.html 2> /dev/null ; \
     then echo index.html found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ > /dev/null
	@ls `pwd`/tmp2/index.html > /dev/null

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/|grep "found local">/dev/null

	@echo prefetch $(SRR) when there is no kfg
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@rm -r $(SRAC)

	@echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg
	@rm -f `pwd`/tmp/sra/$(SRAC).sra
	@echo SRR download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@ls `pwd`/tmp/sra/$(SRAC).sra > /dev/null

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) | grep "found local" > /dev/null

	@echo prefetch $(REFSEQ) when there is no kfg
	@rm -f KC702174.1
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) > /dev/null
	@rm KC702174.1

	@echo '$(PUBLIC)/apps/refseq/volumes/refseq = "refseq"' >> tmp/t.kfg
	@echo REFSEQ HTTP download when user repository is configured
	@if ls `pwd`/tmp/refseq/KC702174.1 2> /dev/null ; \
     then echo KC702174.1 found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) > /dev/null
	@ls `pwd`/tmp/refseq/KC702174.1 > /dev/null

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(KMER) > /dev/null 2>&1 ; \
        then echo "prefetch $(KMER) when there is no kfg should fail"; exit 1; \
	    fi

	@echo '$(PUBLIC)/apps/nakmer/volumes/nakmerFlat = "nannot"' >> tmp/t.kfg
	@echo NANNOT download when user repository is configured
	@if ls `pwd`/tmp/nannot/GCA_000390265.1_R 2> /dev/null ; \
     then echo GCA_000390265.1_R found ; exit 1 ; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) > /dev/null
	@ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(WGS) > /dev/null 2>&1 ; \
        then echo "prefetch $(WGS) when there is no kfg should fail" ; exit 1; \
	    fi

	@echo '$(PUBLIC)/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/t.kfg
	@echo
	@echo WGS HTTP download when user repository is configured
	@if ls `pwd`/tmp/wgs/AFVF01 2> /dev/null ; \
		then echo AFVF01 found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(WGS) > /dev/null
	@echo INCORRECT DOWNLOAD OF WGS SHOULD BE FIXED: VDB-3537
	@ls `pwd`/tmp/wgs/AFVF01 > /dev/null ;
	@echo

	@rm -f `pwd`/tmp/sra/$(SRAC).sra
	@ echo SRR FASP download when user repository is configured IS DISABLED
#	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo SRR FASP download when user repository is configured ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) > /dev/null ; \
	    ls `pwd`/tmp/sra/$(SRAC).sra > /dev/null ; \
	 else echo download of $(SRRF) when ascp is not found is disabled ; \
	 fi

	@rm -f `pwd`/tmp/refseq/KC702174.1
	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo REFSEQ FASP download when user repository is configured ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(REFSEQF) > /dev/null ; \
	    ls `pwd`/tmp/refseq/KC702174.1 > /dev/null ; \
	 else echo download of $(REFSEQF) when ascp is not found is disabled ; \
	 fi

	@rm `pwd`/tmp/nannot/GCA_000390265.1_R
	@echo NANNOT FASP download when user repository is configured
	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo NANNOT FASP download when user repository is configured ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(KMERF) > /dev/null ; \
	      ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null ; \
	 else echo download of $(KMERF) when ascp is not found is disabled ; \
	 fi

	@rm `pwd`/tmp/wgs/AFVF01
	@if ls `pwd`/tmp/wgs/AFVF01 2> /dev/null ; \
     then echo AFVF01 found ; exit 1; fi
	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then echo ; \
	    echo WGS FASP download when user repository is configured ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(WGSF) > /dev/null ; \
	    echo INCORRECT DOWNLOAD OF WGS SHOULD BE FIXED: VDB-3537 ; \
	    ls `pwd`/tmp/wgs/AFVF01 > /dev/null ; \
	 else echo download of $(WGSF) when ascp is not found is disabled ; \
	 fi
	@echo

	@rm -f `pwd`/tmp/sra/$(SRAC).sra
#	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(SRAC) 2> /dev/null ; \
        then echo "prefetch $(SRAC) when there is no kfg should fail" ; \
             exit 1; \
	    fi

	@echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' >> tmp/t.kfg
	@echo SRR accession download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) > /dev/null
	@ls `pwd`/tmp/sra/$(SRAC).sra > /dev/null

	@echo prefetch run with refseqs
	@rm -fr `pwd`/SRR619505
	@rm -fv `pwd`/tmp/sra/SRR619505.sra
	@rm -fv `pwd`/tmp/refseq/NC_000005.8
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR619505 > /dev/null
	@ls `pwd`/tmp/sra/SRR619505.sra > /dev/null
	@ls `pwd`/tmp/refseq/NC_000005.8 > /dev/null

	@echo REFSEQ accession download
	@rm -f `pwd`/tmp/refseq/KC702174.1
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch KC702174.1 > /dev/null
	@ls `pwd`/tmp/refseq/KC702174.1 > /dev/null

	@echo NANNOT accession download
	@rm -f `pwd`/tmp/nannot/GCA_000390265.1_R
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch GCA_000390265.1_R > /dev/null
	@ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null

	@echo WGS accession download
	@rm -f `pwd`/tmp/wgs/AFVF01
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch AFVF01 > /dev/null
	@ls `pwd`/tmp/wgs/AFVF01 > /dev/null

	@if ls `pwd`/tmp2/100MB 2> /dev/null ; \
     then echo 100MB found ; exit 1; fi
	@ echo FASP download: asperasoft/100MB: turned off
#	@if ascp -h > /dev/null ; \
     then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch $(LARGE) > /dev/null && \
	    ls `pwd`/100MB > /dev/null ; \
	 else \
	    if $(BINDIR)/prefetch $(LARGE) 2> /dev/null ; \
        then \
            echo "prefetch <FASP URL> when ascp is not found should fail" ; \
            exit 1; \
	     fi ; \
	 fi

	@if ascp -h > /dev/null ; \
     then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 else \
	    if $(BINDIR)/prefetch $(LARGE) 2> /dev/null ; \
        then \
            echo "prefetch <FASP URL> when ascp is not found should fail" ; \
            exit 1; \
	     fi ; \
	 fi

	@rm -f `pwd`/tmp2/100MB
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if $(BINDIR)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:100MB \
                                                             > /dev/null 2>&1; \
	    then echo "prefetch <FASP> when FASP source is not found should fail"; \
             exit 1 ; \
	    fi

	@echo vdbcache download
	@rm tmp/sra/*
	@if ls `pwd`/tmp/sra/SRR6667190.sra 2> /dev/null ; \
		then echo SRR6667190.sra found ; exit 1; fi
	@if ls `pwd`/tmp/sra/SRR6667190.sra.vdbcache 2> /dev/null ; \
		then echo SRR6667190.sra.vdbcache found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
		$(BINDIR)/prefetch SRR6667190 > /dev/null
	@ls `pwd`/tmp/sra/SRR6667190.sra          > /dev/null
	@ls `pwd`/tmp/sra/SRR6667190.sra.vdbcache > /dev/null

	@rm -r tmp*

################################################################################

ncbi1GB:
	@mkdir -p tmp

	@echo 'foo = "bar"' > tmp/t.kfg

	@if ls `pwd`/tmp/1GB 2> /dev/null; \
     then echo 1GB found ; exit 1; fi

	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	  echo FASP download: ncbi/1GB ; \
	  export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp ; \
	    $(BINDIR)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:1GB > /dev/null;\
	    ls `pwd`/1GB > /dev/null ; \
	 fi

	@rm -r tmp

################################################################################

out_dir_and_file:
	@ mkdir -p tmp tmp2
	@ rm -fr tmp*/*
	@ echo  version 1.0     >  tmp/k
	@ echo '0||SRR053325||' >> tmp/k
	@ echo '$$end'          >> tmp/k

	@ echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' >  tmp/t.kfg
	@ echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"'         >> tmp/t.kfg
	@ echo '$(PUBLIC)/root = "$(CWD)/tmp"'                      >> tmp/t.kfg
	@ echo '/repository/site/disabled = "true"'                 >> tmp/t.kfg

	@ echo PREFETCH ACCESSION TO OUT-FILE
	@ rm -f `pwd`/tmp-file*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) -o tmp-file -v > /dev/null
	@ ls `pwd`/tmp-file > /dev/null
	@ rm `pwd`/tmp-file

	@ echo PREFETCH ACCESSION TO OUT-FILE INSIDE OF DIR
	@ if ls `pwd`/tmp3/dir/file 2> /dev/null ; \
      then echo file found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) -O / -o tmp3/dir/file -v > /dev/null
	@ ls `pwd`/tmp3/dir/file > /dev/null

	@ echo PREFETCH SRR HTTP URL TO OUT-FILE
	@ rm -f `pwd`/tmp3/dir/file
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) -O / -o tmp3/dir/file > /dev/null
	@ ls `pwd`/tmp3/dir/file > /dev/null

	@ echo PREFETCH SRR FASP URL TO OUT-FILE IS DISABLED
#	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo PREFETCH SRR FASP URL TO OUT-FILE ; \
	    rm -f `pwd`/tmp3/dir/file ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) -O / -o tmp3/dir/file > /dev/null ; \
	    ls `pwd`/tmp3/dir/file > /dev/null ; \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo
#	@ echo

	@ echo PREFETCH HTTP DIRECTORY URL TO OUT-FILE
	@ rm -f `pwd`/tmp3/dir/file
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ -O / -o tmp3/dir/file \
	        > /dev/null
	@ ls `pwd`/tmp3/dir/file > /dev/null
#	@ echo

	@ echo PREFETCH HTTP FILE URL TO OUT-FILE
	@ rm -f `pwd`/tmp3/dir/file
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki -O / \
	                       -o tmp3/dir/file > /dev/null
	@ ls `pwd`/tmp3/dir/file > /dev/null
#	@ echo

	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo PREFETCH FASP URL TO OUT-FILE ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    rm -f `pwd`/tmp3/dir/file ; \
	    $(BINDIR)/prefetch $(REFSEQF) -O / -o tmp3/dir/file > /dev/null ; \
	    ls `pwd`/tmp3/dir/file > /dev/null ; \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi

	@ if [ "$(BUILD)" = "dbg" ]; then \
	    echo PREFETCH TEXT-KART was disabled: need to have a public example of protected run or add support of public accessions; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    echo $(BINDIR)/prefetch --text-kart tmp/k > /dev/null ; \
	    echo ls `pwd`/tmp/sra/SRR053325.sra > /dev/null ; \
	  fi

	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	  if $(BINDIR)/prefetch SRR045450 $(SRAC) -o tmp/o 2> /dev/null ; \
	  then echo unexpected success when downloading multiple items to file ; \
	      exit 1 ; \
	  fi ;

	@ echo PREFETCH MULTIPLE ITEMS
	@ rm -fr `pwd`/SRR0* `pwd`/tmp/sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR045450 $(SRAC) > /dev/null
	@ ls `pwd`/tmp/sra/SRR045450.sra `pwd`/tmp/sra/$(SRAC).sra > /dev/null

	@ echo PREFETCH SRR HTTP URL
	@ rm `pwd`/tmp/sra/$(SRAC).sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@ ls `pwd`/tmp/sra/$(SRAC).sra > /dev/null

	@ echo PREFETCH SRR FASP URL IS DISABLED
#	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo PREFETCH SRR FASP URL ; \
	    rm `pwd`/tmp/sra/$(SRAC).sra ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) > /dev/null ; \
	    ls `pwd`/tmp/sra/$(SRAC).sra > /dev/null ; \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi

	@ echo PREFETCH HTTP DIRECTORY URL
	@ if ls `pwd`/tmp2/index.html 2> /dev/null ; then exit 1; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ > /dev/null
	@ ls `pwd`/tmp2/index.html > /dev/null

	@ echo PREFETCH HTTP FILE URL
	@ if ls `pwd`/tmp2/wiki 2> /dev/null ; \
      then echo wiki found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@ ls `pwd`/tmp2/wiki > /dev/null

	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo PREFETCH FASP URL ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if ls `pwd`/KC702174.1 2> /dev/null ; then exit 1; fi ; \
	    $(BINDIR)/prefetch $(REFSEQF) > /dev/null && \
	    ls `pwd`/KC702174.1 > /dev/null ; \
	  else echo download of FASP URL when ascp is not found is disabled ; \
	  fi

	@ echo PREFETCH ACCESSION TO OUT-DIR
	@ rm -f `pwd`/tmp3/dir/$(SRAC).sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) -O tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/$(SRAC).sra > /dev/null

	@ echo PREFETCH SRR HTTP URL TO OUT-DIR
	@ rm -f `pwd`/tmp3/dir/$(SRAC).sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) -O tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/$(SRAC).sra > /dev/null

	@ echo PREFETCH SRR FASP URL TO OUT-DIR IS DISABLED
#	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo PREFETCH SRR FASP URL TO OUT-DIR ; \
	    rm -f `pwd`/tmp3/dir/$(SRAC).sra ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) -O tmp3/dir > /dev/null ; \
	    ls `pwd`/tmp3/dir/$(SRAC).sra > /dev/null ; \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ echo PREFETCH HTTP DIRECTORY URL TO OUT-DIR
	@ rm -f index.html
	@ if ls `pwd`/tmp3/dir/index.html 2> /dev/null ; \
      then echo index.html found ; exit 1 ; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ -O tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/index.html > /dev/null
#	@ echo

	@ echo PREFETCH HTTP FILE URL TO OUT-DIR
	@ rm -f wiki
	@ if ls `pwd`/tmp3/dir/wiki 2> /dev/null ; \
      then echo wiki found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki -O tmp3/dir \
	                       > /dev/null
	@ ls `pwd`/tmp3/dir/wiki > /dev/null
#	@ echo

	@ if [ "${HAVE_NCBI_ASCP}" = "1" ] ; then \
	    echo PREFETCH FASP URL TO OUT-DIR ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if ls `pwd`/tmp3/dir/KC702174.1 2> /dev/null ; then \
        echo 100MB found ; exit 1 ; fi ; \
	    $(BINDIR)/prefetch $(REFSEQF) -O tmp3/dir > /dev/null && \
	    ls `pwd`/tmp3/dir/KC702174.1 > /dev/null ; \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ rm -r tmp*

s-option:
	@ mkdir -p tmp
	@ echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' > tmp/t.kfg
	@ echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg
	@ echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg

	@ echo Downloading KART was disabled: need to have a public example of protected run or add support of public accessions
	@ rm -f tmp/sra/SRR0*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	#	$(BINDIR)/prefetch kart.krt > /dev/null

	@ echo Downloading KART: ORDER BY SIZE / SHORT OPTION disabled, too
	@ rm -f tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	#	$(BINDIR)/prefetch kart.krt -os > /dev/null

	@ echo Downloading KART: ORDER BY KART / SHORT OPTION disabled, too
	@ rm -f tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	#	$(BINDIR)/prefetch kart.krt -ok > /dev/null

	@ echo Downloading KART: ORDER BY SIZE / LONG OPTION disabled, too
	@ rm -f tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	#	$(BINDIR)/prefetch kart.krt --order s > /dev/null

	@ echo Downloading KART: ORDER BY KART / LONG OPTION disabled, too
	@ rm -f tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	#	$(BINDIR)/prefetch kart.krt --order k > /dev/null

	@ echo Downloading ACCESSION
	@ rm -f tmp/sra/$(SRAC).sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) > /dev/null

	@ echo Downloading ACCESSION TO FILE: SHORT OPTION
	@ rm -f tmp/1
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) -otmp/1 > /dev/null
#	@ echo

	@ echo Downloading ACCESSION TO FILE: LONG OPTION
	@ rm -f tmp/2
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) --output-file tmp/2 > /dev/null

	@ echo Downloading ACCESSIONS
	@ rm -f tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRAC) SRR045450 > /dev/null

	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	  if $(BINDIR)/prefetch SRR045450 $(SRAC) -o tmp/o 2> /dev/null ; \
	  then echo unexpected success when downloading multiple items to file ; \
	      exit 1 ; \
	  fi ;

	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	  if $(BINDIR)/prefetch SRR045450 $(SRAC) -output-file tmp/o \
                                                                2> /dev/null ; \
	  then echo unexpected success when downloading multiple items to file ; \
	      exit 1 ; \
	  fi ;

	@ echo  version 1.0     >  tmp/k
	@ echo '0||SRR045450||' >> tmp/k
	@ echo '0||SRR053325||' >> tmp/k
	@ echo '$$end'          >> tmp/k

	@ if [ "$(BUILD)" = "dbg" ]; then \
	echo Downloading TEXT KART is disabled, as well ; \
    if [ "$(BUILD)" = "dg" ]; then \
	      echo Downloading TEXT KART && \
	      rm -f tmp/sra/* && \
	      export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
	      $(BINDIR)/prefetch --text-kart tmp/k > /dev/null && \
          \
	      echo Downloading TEXT KART: ORDER BY SIZE / SHORT OPTION && \
	      rm -f tmp/sra/* && \
	      export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
	      $(BINDIR)/prefetch --text-kart tmp/k -os > /dev/null 2>&1 && \
          \
	      echo Downloading TEXT KART: ORDER BY KART / SHORT OPTION && \
	      rm -f tmp/sra/* && \
	      export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
	      $(BINDIR)/prefetch --text-kart tmp/k -ok > /dev/null 2>&1 && \
          \
	      echo Downloading TEXT KART: ORDER BY SIZE / LONG OPTION && \
	      rm -f tmp/sra/* && \
	      export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
	      $(BINDIR)/prefetch --text-kart tmp/k --order s > /dev/null && \
          \
	      echo Downloading TEXT KART: ORDER BY KART / LONG OPTION && \
	      rm -f tmp/sra/* && \
	      export VDB_CONFIG=`pwd`/tmp && export NCBI_SETTINGS=/ && \
	      $(BINDIR)/prefetch --text-kart tmp/k --order k > /dev/null ; \
	  fi \
	  fi

	@ rm -r tmp*

truncated:
	@ echo prefetch correctly re-downloads incomplete files

#setup
	@ mkdir -p tmp/sra
	@ echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' > tmp/t.kfg
	@ echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"'        >> tmp/t.kfg
	@ echo '$(PUBLIC)/root = "$(CWD)/tmp"'                     >> tmp/t.kfg
#	@ ls -l tmp	@ cat tmp/t.kfg
#	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ $(BINDIR)/vdb-config -on
#	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ $(BINDIR)/srapath $(SRAC)
#	echo 669	perl check-size.pl 4	echo 671

# create truncated run
	@ echo '123' > tmp/sra/$(SRAC).sra
	@ if perl check-size.pl 4 > /dev/null; \
	  then echo "run should be truncated for this test"; exit 1; \
	  fi

# prefetch should detect truncated run and redownload it
	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ $(BINDIR)/prefetch $(SRAC) > tmp/out
	@ if grep ": 1) '$(SRAC)' is found locally" tmp/out ; \
	  then echo "prefetch incorrectly stopped on truncated run"; exit 1; \
	  fi
#	@ wc tmp/sra/$(SRAC).sra	@ wc -c tmp/sra/$(SRAC).sra

# run should be complete
	@ perl check-size.pl 4

# the second run of prefetch should find local file
	@ VDB_CONFIG=`pwd`/tmp NCBI_SETTINGS=/ $(BINDIR)/prefetch $(SRAC) > tmp/out
	@ grep -q ": 1) '$(SRAC)' is found locally" tmp/out
	@ perl check-size.pl 4

#cleanup
	@ rm -r tmp

kart: ##########################################################################
	@rm -frv tmp/*
	@mkdir -p tmp
	@echo 'repository/remote/main/SDL.2/resolver-cgi = "$(SDL)"' > tmp/t.kfg

	@echo "Downloading --cart <kart> ordered by default (size)"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc --cart ../3-dbGaP-0.krt -Cn"
	@cd tmp && rm     SRR1219879/SRR1219879_dbGaP-0.sra
	@cd tmp && rm     SRR1219880/SRR1219880_dbGaP-0.sra
	@cd tmp && rm     SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir  SRR1219879 SRR1219880 SRR1257493

	@echo "Downloading kart ordered by default (size)"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -Cn"
	@cd tmp && rm     SRR1219879/SRR1219879_dbGaP-0.sra
	@cd tmp && rm     SRR1219880/SRR1219880_dbGaP-0.sra
	@cd tmp && rm     SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir  SRR1219879 SRR1219880 SRR1257493

	@echo "Downloading kart ordered by size"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -os -Cn"
	@cd tmp && rm     SRR1219879/SRR1219879_dbGaP-0.sra
	@cd tmp && rm     SRR1219880/SRR1219880_dbGaP-0.sra
	@cd tmp && rm     SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir  SRR1219879 SRR1219880 SRR1257493

	@echo "Downloading kart ordered by kart"
	@cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -ok -Cn"
	@rm tmp/SRR1219879/SRR1219879_dbGaP-0.sra
	@rm tmp/SRR1219880/SRR1219880_dbGaP-0.sra
	@rm tmp/SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir SRR1219879 SRR1219880 SRR1257493

	@rm -frv tmp/sra
	@echo 'repository/remote/main/SDL.2/resolver-cgi = "$(SDL)"' > tmp/t.kfg
	@echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg
	@echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg

	@echo "Downloading kart ordered by default (size) to user repo"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
      "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -Cn"
	@rm    tmp/sra/SRR1219879_dbGaP-0.sra
	@rm    tmp/sra/SRR1219880_dbGaP-0.sra
	@rm    tmp/sra/SRR1257493_dbGaP-0.sra
	@rmdir tmp/sra

	@echo "Downloading kart ordered by size to user repo"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -os -Cn"
	@rm    tmp/sra/SRR1219879_dbGaP-0.sra
	@rm    tmp/sra/SRR1219880_dbGaP-0.sra
	@rm    tmp/sra/SRR1257493_dbGaP-0.sra
	@rmdir tmp/sra

	@echo "Downloading kart ordered by kart to user repo"
	@cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -ok -Cn"
	@rm    tmp/sra/SRR1219879_dbGaP-0.sra
	@rm    tmp/sra/SRR1219880_dbGaP-0.sra
	@rm    tmp/sra/SRR1257493_dbGaP-0.sra
	@rmdir tmp/sra

	@echo '/tools/prefetch/download_to_cache = "false"' >> tmp/t.kfg
	@echo "Downloading kart ordered by default (size) ignoring user repo"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -Cn"
	@cd tmp && rm     SRR1219879/SRR1219879_dbGaP-0.sra
	@cd tmp && rm     SRR1219880/SRR1219880_dbGaP-0.sra
	@cd tmp && rm     SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir  SRR1219879 SRR1219880 SRR1257493

	@echo "Downloading kart ordered by size ignoring user repo"
	@cd tmp && printf "ding 'SRR1219879'\nding 'SRR1219880'\nding 'SRR1257493'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -os -Cn"
	@cd tmp && rm     SRR1219879/SRR1219879_dbGaP-0.sra
	@cd tmp && rm     SRR1219880/SRR1219880_dbGaP-0.sra
	@cd tmp && rm     SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir  SRR1219879 SRR1219880 SRR1257493

	@echo "Downloading kart ordered by kart ignoring user repo"
	@cd tmp && printf "ding 'SRR1257493'\nding 'SRR1219880'\nding 'SRR1219879'"|\
	 NCBI_SETTINGS=/ VDB_CONFIG=. \
	 perl ../check-prefetch-out.pl 0 0 \
	  "$(BINDIR)/prefetch --ngc ../prj_phs710EA_test.ngc ../3-dbGaP-0.krt -ok -Cn"
	@rm tmp/SRR1219879/SRR1219879_dbGaP-0.sra
	@rm tmp/SRR1219880/SRR1219880_dbGaP-0.sra
	@rm tmp/SRR1257493/SRR1257493_dbGaP-0.sra
	@cd tmp && rmdir SRR1219879 SRR1219880 SRR1257493

	@rm -r tmp

wgs:
	@echo Verifying prefetch of runs with WGS references...

	@rm -fr tmp
	@mkdir  tmp

	@#pwd
	@echo '/repository/site/disabled = "true"' >  tmp/k
	@echo '/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"' >> tmp/k
	@echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' \
		>> tmp/k
	@echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/k
	@printf '/repository/user/main/public/root = "%s/tmp"\n' `pwd` >> tmp/k
	@#cat tmp/k

	@cd tmp && NCBI_SETTINGS=/ $B/prefetch SRR619505  -fy     > /dev/null
	@ls tmp/SRR619505/SRR619505.sra tmp/SRR619505/NC_000005.8 > /dev/null
	@rm -r tmp/S*

	@cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $B/prefetch SRR619505  -fy     > /dev/null
	@ls tmp/SRR619505/SRR619505.sra tmp/refseq/NC_000005.8    > /dev/null
	@rm -r tmp/[rS]*

#TODO: TO ADD AD for AAAB01 cd tmp &&                 prefetch SRR353827 -fy -+VFS

	@cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $B/prefetch SRR353827  -fy     > /dev/null
	@cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $B/align-info SRR353827 \
		| grep -v 'false,remote::https:'                      > /dev/null
	@ls tmp/SRR353827/SRR353827.sra tmp/wgs/AAAB01            > /dev/null
	@rm -r tmp/[wS]*

	@echo  Verifying prefetch of run with with a lot of WGS references...
	@echo '      ... prefetching...'
	@cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $B/prefetch   ERR3091357 -fy   > /dev/null
	@echo '      ... align-infoing...'
	@cd tmp && NCBI_SETTINGS=/ VDB_CONFIG=k $B/align-info ERR3091357 \
		| grep -v 'false,remote::https:'                      > /dev/null
	@ls tmp/ERR3091357/ERR3091357.sra tmp/wgs/JTFH01 tmp/refseq/KN707955.1 \
	                                                          > /dev/null

	@rm -r tmp

resume:
	@ echo Verifying prefetch resume
	@ rm   -frv tmp/*
	@ mkdir -p  tmp
	@ echo '/repository/site/disabled = "true"' >  tmp/k
	@ cd tmp && PATH=$(B):$(PATH) NCBI_SETTINGS=k perl ../test-resume.pl
	@ rm    -r  tmp

hs37d5:
	@ echo Verifying hs37d5
	@ rm   -frv tmp/*
	@ mkdir -p  tmp
	@ cd tmp && PATH=$(B):$(PATH) NCBI_SETTINGS=k perl ../test-hs37d5.pl
	@ rm    -r  tmp

ad_as_dir: # turned off by default. Library test is in 
           # ncbi-vdb\test\vfs\managertest.cpp::TestVFSManagerCheckAd
	@ echo we can use AD as dir/...
	@ mkdir -p tmp
	@ export NCBI_SETTINGS=\ && export PATH=$(SRATOOLS_BINDIR):$(PATH) \
		&& cd tmp && perl ../use-AD-as-dir.pl
	@ rm -r tmp
