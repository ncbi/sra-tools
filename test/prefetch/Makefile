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
CGI=https://www.ncbi.nlm.nih.gov/Traces/names/names.fcgi
LARGE=fasp://aspera@demo.asperasoft.com:aspera-test-dir-large/100MB
SRA=https://sra-download.ncbi.nlm.nih.gov
SRAF=fasp://dbtest@sra-download.ncbi.nlm.nih.gov
SRAF=fasp://dbtest@srafiles31-1.st-va.ncbi.nlm.nih.gov
KMER=$(SRA)/traces/nannot01/kmer/000/390/GCA_000390265.1_R
KMERF=$(SRAF):data/sracloud/traces/nannot01/kmer/000/390/GCA_000390265.1_R
REFSEQ=$(SRA)/traces/refseq/KC702174.1
REFSEQF=$(SRAF):data/sracloud/traces/refseq/KC702174.1
SRR=$(SRA)/traces/sra59/SRR/000052/SRR053325
SRRF=$(SRAF):data/sracloud/traces/sra59/SRR/000052/SRR053325
WGS=$(SRA)/traces/wgs03/WGS/AF/VF/AFVF01.1
WGSF=$(SRAF):data/sracloud/traces/wgs03/WGS/AF/VF/AFVF01.1

runtests: urls_and_accs out_dir_and_file s-option

################################################################################
urls_and_accs:
	@mkdir -p tmp tmp2
	@rm -fr tmp/* tmp2/*
	@echo 'foo = "bar"' > tmp/t.kfg

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki 2>/dev/null ; \
        then echo "prefetch URL-1 when there is no kfg should fail"; exit 1; \
	    fi

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/ 2>/dev/null ; \
        then echo "prefetch URL-2 when there is no kfg should fail"; exit 1; \
	    fi

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if ping -c1 intranet > /dev/null ; then \
	       if $(BINDIR)/prefetch http://intranet/ 2>/dev/null ; \
	       then echo "prefetch URL-2 when there is no kfg should fail"; exit 1; \
	       fi ; \
	    fi

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if ping -c1 intranet > /dev/null ; then \
	     if $(BINDIR)/prefetch http://intranet/irb/support.shtml  2>/dev/null ; \
	     then echo "prefetch URL-2 when there is no kfg should fail"; exit 1; \
	     fi ; \
	    fi

	@echo '$(PUBLIC)/apps/file/volumes/flat = "files"' > tmp/t.kfg
	@echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg

	@if ls `pwd`/tmp2/support.shtml 2> /dev/null ; \
     then echo support.shtml found ; exit 1 ; fi
	@echo HTTP download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if ping -c1 intranet > /dev/null ; then \
	    $(BINDIR)/prefetch http://intranet/irb/support.shtml > /dev/null ; fi
	@if ping -c1 intranet > /dev/null ; \
	 then ls `pwd`/tmp2/support.shtml > /dev/null ; fi
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 if ping -c1 intranet > /dev/null ; then \
	     $(BINDIR)/prefetch http://intranet/irb/support.shtml \
	        | grep "found local" > /dev/null ; fi
#	@echo

	@if ls `pwd`/tmp2/index.html 2> /dev/null ; \
     then echo index.shtml found ; exit 1; fi
	@echo HTTP download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 if ping -c1 intranet > /dev/null ; then \
	    $(BINDIR)/prefetch http://intranet/ > /dev/null ; fi
	@if ping -c1 intranet>/dev/null; then ls `pwd`/tmp2/index.html > /dev/null;fi
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	 if ping -c1 intranet > /dev/null ; then \
	    $(BINDIR)/prefetch http://intranet/ | grep "found local" > /dev/null ; fi
	@rm -f tmp2/index.html
#	@echo

	@echo URL download when user repository is configured
	@if ls `pwd`/tmp2/wiki 2> /dev/null ; then echo wiki found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@ls `pwd`/tmp2/wiki > /dev/null
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki \
	                              | grep "found local" > /dev/null
#	@echo

	@echo URL/ download when user repository is configured
	@if ls `pwd`/tmp2/index.html 2> /dev/null ; \
     then echo index.html found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ > /dev/null
	@ls `pwd`/tmp2/index.html > /dev/null
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/|grep "found local">/dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(SRR) 2> /dev/null ; \
        then echo "prefetch $(SRR) when there is no kfg should fail"; exit 1; \
	    fi

	@echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg
	@if ls `pwd`/tmp/sra/SRR053325.sra 2> /dev/null ; \
     then echo SRR053325.sra found ; exit 1; fi
	@echo SRR download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@ls `pwd`/tmp/sra/SRR053325.sra > /dev/null
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(REFSEQ) 2> /dev/null ; \
        then echo "prefetch $(SRR) when there is no kfg should fail" ; exit 1; \
	    fi

	@echo '$(PUBLIC)/apps/refseq/volumes/refseq = "refseq"' >> tmp/t.kfg
	@echo REFSEQ download when user repository is configured
	@if ls `pwd`/tmp/refseq/KC702174.1 2> /dev/null ; \
     then echo KC702174.1 found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) > /dev/null
	@ls `pwd`/tmp/refseq/KC702174.1 > /dev/null
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(KMER) 2> /dev/null ; \
        then echo "prefetch $(KMER) when there is no kfg should fail"; exit 1; \
	    fi

	@echo '$(PUBLIC)/apps/nakmer/volumes/nakmerFlat = "nannot"' >> tmp/t.kfg
	@echo NANNOT download when user repository is configured
	@if ls `pwd`/tmp/nannot/GCA_000390265.1_R 2> /dev/null ; \
     then echo GCA_000390265.1_R found ; exit 1 ; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) > /dev/null
	@ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null
#	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(WGS) 2> /dev/null; \
        then echo "prefetch $(WGS) when there is no kfg should fail" ; exit 1; \
	    fi

	@echo '$(PUBLIC)/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/t.kfg
	@echo WGS download when user repository is configured
	@if ls `pwd`/tmp/wgs/AFVF01 2> /dev/null ; \
     then echo AFVF01 found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(WGS) > /dev/null
	@echo INCORRECT DOWNLOAD OF WGS SHOULD BE FIXED: VDB-3537
	@ls `pwd`/tmp/wgs/AFVF01 > /dev/null ;
	@echo

	@rm `pwd`/tmp/sra/SRR053325.sra
	@echo SRR FASP download when user repository is configured
	if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) > /dev/null ; \
	    ls `pwd`/tmp/sra/SRR053325.sra > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	 else echo download of $(SRRF) when ascp is not found is disabled ; \
	 fi
#	@echo

	@rm `pwd`/tmp/refseq/KC702174.1
	@echo REFSEQ download when user repository is configured
	@if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(REFSEQF) > /dev/null ; \
	    ls `pwd`/tmp/refseq/KC702174.1 > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	 else echo download of $(REFSEQF) when ascp is not found is disabled ; \
	 fi
#	@echo

	@rm `pwd`/tmp/nannot/GCA_000390265.1_R
	@echo NANNOT FASP download when user repository is configured
	@if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(KMERF) > /dev/null ; \
	      ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	 else echo download of $(KMERF) when ascp is not found is disabled ; \
	 fi
#	@echo

	@rm `pwd`/tmp/wgs/AFVF01
	@echo WGS download when user repository is configured
	@if ls `pwd`/tmp/wgs/AFVF01 2> /dev/null ; \
     then echo AFVF01 found ; exit 1; fi
	@if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(WGSF) > /dev/null ; \
	    echo INCORRECT DOWNLOAD OF WGS SHOULD BE FIXED: VDB-3537 ; \
	    ls `pwd`/tmp/wgs/AFVF01 > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	 else echo download of $(WGSF) when ascp is not found is disabled ; \
	 fi
	@echo

	@rm -f `pwd`/tmp/sra/SRR053325.sra
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch SRR053325 2> /dev/null ; \
        then echo "prefetch SRR053325 when there is no kfg should fail" ; \
             exit 1; \
	    fi

	@echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' >> tmp/t.kfg
	@echo SRR accession download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 > /dev/null
	@ls `pwd`/tmp/sra/SRR053325.sra > /dev/null
#	@echo

	@echo prefetch run with refseqs
	@if ls `pwd`/tmp/sra/SRR619505.sra 2> /dev/null ; \
     then echo SRR619505 found ; exit 1; fi
	@if ls `pwd`/tmp/refseq/NC_000005.8 2> /dev/null ; \
     then echo NC_000005.8 found ; exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR619505 > /dev/null
	@ls `pwd`/tmp/sra/SRR619505.sra > /dev/null
	@ls `pwd`/tmp/refseq/NC_000005.8 > /dev/null

	@echo REFSEQ accession download
	@rm -f `pwd`/tmp/refseq/KC702174.1
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch KC702174.1 > /dev/null
	@ls `pwd`/tmp/refseq/KC702174.1 > /dev/null
#	@echo

	@echo NANNOT accession download
	@rm -f `pwd`/tmp/nannot/GCA_000390265.1_R
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch GCA_000390265.1_R > /dev/null
	@ls `pwd`/tmp/nannot/GCA_000390265.1_R > /dev/null
#	@echo

	@echo WGS accession download
	@rm -f `pwd`/tmp/wgs/AFVF01
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch AFVF01 > /dev/null
	@ls `pwd`/tmp/wgs/AFVF01 > /dev/null
#	@echo

	@echo FASP download: asperasoft/100MB
	@if ls `pwd`/tmp2/100MB 2> /dev/null ; \
     then echo 100MB found ; exit 1; fi
	@if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch $(LARGE) > /dev/null ; \
	    ls `pwd`/100MB > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	 else \
	  if $(BINDIR)/prefetch $(LARGE) 2> /dev/null ; \
      then echo "prefetch <FASP URL> when ascp is not found should fail" ; \
           exit 1; \
	  fi ; \
	 fi
#	@echo

	@echo FASP download: ncbi/1GB
	@if ls `pwd`/tmp2/1GB 2> /dev/null; \
     then echo 1GB found ; exit 1; fi
	@if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	  export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:1GB > /dev/null;\
	    ls `pwd`/1GB > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	 fi
#	@echo

	@rm -f `pwd`/tmp2/100MB
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if $(BINDIR)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:100MB \
                                                             > /dev/null 2>&1; \
	    then echo "prefetch <FASP> when FASP source is not found should fail"; \
             exit 1 ; \
	    fi

#	export VDB_CONFIG=`pwd`/tmp;export NCBI_SETTINGS=/; $(BINDIR)/vdb-config -on

	@rm -r tmp*

################################################################################

out_dir_and_file:
#	@ echo
	@ mkdir -p tmp tmp2
	@ rm -fr tmp*/*
	@ echo  version 1.0     >  tmp/k
	@ echo '0||SRR053325||' >> tmp/k
	@ echo '$$end'          >> tmp/k
#	@ echo
	@ echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' >  tmp/t.kfg
	@ echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"'         >> tmp/t.kfg
	@ echo '$(PUBLIC)/root = "$(CWD)/tmp"'                      >> tmp/t.kfg

	@ echo PREFETCH ACCESSION TO OUT-FILE
	@ if ls `pwd`/tmp3/dir/file 2> /dev/null ; \
      then echo file found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 -O / -o tmp3/dir/file -v > /dev/null
	@ ls `pwd`/tmp3/dir/file > /dev/null
#	@ echo

	@ echo PREFETCH SRR HTTP URL TO OUT-FILE
	@ rm -f `pwd`/tmp3/dir/file
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) -O / -o tmp3/dir/file > /dev/null
	@ ls `pwd`/tmp3/dir/file > /dev/null
#	@ echo

	@ if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    echo PREFETCH SRR FASP URL TO OUT-FILE ; \
	    rm -f `pwd`/tmp3/dir/file ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) -O / -o tmp3/dir/file > /dev/null ; \
	    ls `pwd`/tmp3/dir/file > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
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

	@ if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    echo PREFETCH FASP URL TO OUT-FILE ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    rm -f `pwd`/tmp3/dir/file ; \
	    $(BINDIR)/prefetch $(LARGE) -O / -o tmp3/dir/file > /dev/null ; \
	    ls `pwd`/tmp3/dir/file > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ if [ "$(BUILD)" = "dbg" ]; then \
	    echo PREFETCH TEXT-KART ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    rm -f `pwd`/tmp/sra/SRR053325.sra ; \
	    $(BINDIR)/prefetch --text-kart tmp/k > /dev/null ; \
	    ls `pwd`/tmp/sra/SRR053325.sra > /dev/null ; \
	  fi

	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	  if $(BINDIR)/prefetch SRR045450 SRR053325 -o tmp/o 2> /dev/null ; \
	  then echo unexpected success when downloading multiple items to file ; \
	      exit 1 ; \
	  fi ;

	@ echo PREFETCH MULTIPLE ITEMS
	@ if ls `pwd`/tmp/sra/SRR045450.sra 2> /dev/null ; \
      then echo SRR045450.sra found ; exit 1 ; fi ;
	@ rm -f `pwd`/tmp/sra/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR045450 SRR053325 > /dev/null
	@ ls `pwd`/tmp/sra/SRR045450.sra `pwd`/tmp/sra/SRR053325.sra > /dev/null
#	@ echo

	@ echo PREFETCH SRR HTTP URL
	@ rm `pwd`/tmp/sra/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@ ls `pwd`/tmp/sra/SRR053325.sra > /dev/null
#	@ echo

	@ if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    echo PREFETCH SRR FASP URL ; \
	    rm `pwd`/tmp/sra/SRR053325.sra ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) > /dev/null ; \
	    ls `pwd`/tmp/sra/SRR053325.sra > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ echo PREFETCH HTTP DIRECTORY URL
	@ if ls `pwd`/tmp2/index.html 2> /dev/null ; then exit 1; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ > /dev/null
	@ ls `pwd`/tmp2/index.html > /dev/null
#	@ echo

	@ echo PREFETCH HTTP FILE URL
	@ if ls `pwd`/tmp2/wiki 2> /dev/null ; \
      then echo wiki found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@ ls `pwd`/tmp2/wiki > /dev/null
#	@ echo

	@ if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    echo PREFETCH FASP URL ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if ls `pwd`/100MB 2> /dev/null ; then exit 1; fi ; \
	    $(BINDIR)/prefetch $(LARGE) > /dev/null ; \
	    ls `pwd`/100MB > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ echo PREFETCH ACCESSION TO OUT-DIR
	@ rm -f `pwd`/tmp3/dir/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 -O tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/SRR053325.sra > /dev/null
#	@ echo

	@ echo PREFETCH SRR HTTP URL TO OUT-DIR
	@ rm -f `pwd`/tmp3/dir/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) -O tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/SRR053325.sra > /dev/null
#	@ echo

	@ if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    echo PREFETCH SRR FASP URL TO OUT-DIR ; \
	    rm -f `pwd`/tmp3/dir/SRR053325.sra ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) -O tmp3/dir > /dev/null ; \
	    ls `pwd`/tmp3/dir/SRR053325.sra > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ echo PREFETCH HTTP DIRECTORY URL TO OUT-DIR
	@ if ls `pwd`/tmp3/dir/index.html 2> /dev/null ; \
      then echo index.html found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ -O tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/index.html > /dev/null
#	@ echo

	@ echo PREFETCH HTTP FILE URL TO OUT-DIR
	@ if ls `pwd`/tmp3/dir/wiki 2> /dev/null ; \
      then echo wiki found ; exit 1 ; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki -O tmp3/dir \
	                       > /dev/null
	@ ls `pwd`/tmp3/dir/wiki > /dev/null
#	@ echo

	@ if ascp -h > /dev/null ; then \
	  if [ `hostname` != 'iebdev21' ] ; then \
	    echo PREFETCH FASP URL TO OUT-DIR ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if ls `pwd`/tmp3/dir/100MB 2> /dev/null ; then \
        echo 100MB found ; exit 1 ; fi ; \
	    $(BINDIR)/prefetch $(LARGE) -O tmp3/dir > /dev/null ; \
	    ls `pwd`/tmp3/dir/100MB > /dev/null ; \
	  else echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD HOST; echo BAD;\
	  fi \
	  else echo download of SRR FASP URL when ascp is not found is disabled ; \
	  fi
#	@ echo

	@ rm -r tmp*

s-option:
	@ mkdir -p tmp
	@ echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' > tmp/t.kfg
	@ echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg
	@ echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg
#	@ echo

	@ echo DOWNLOADING KART
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch kart.krt > /dev/null
#	@ echo

	@ echo DOWNLOADING KART: ORDER BY SIZE / SHORT OPTION
	@ rm tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch kart.krt -os > /dev/null
#	@ echo

	@ echo DOWNLOADING KART: ORDER BY KART / SHORT OPTION
	@ rm tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch kart.krt -ok > /dev/null
#	@ echo

	@ echo DOWNLOADING KART: ORDER BY SIZE / LONG OPTION
	@ rm tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch kart.krt --order s > /dev/null
#	@ echo

	@ echo DOWNLOADING KART: ORDER BY KART / LONG OPTION
	@ rm tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch kart.krt --order k > /dev/null
#	@ echo

	@ echo DOWNLOADING ACCESSION
	@ rm tmp/sra/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 > /dev/null
#	@ echo

	@ echo DOWNLOADING ACCESSION TO FILE: SHORT OPTION
	@ rm -f tmp/1
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 -otmp/1 > /dev/null
#	@ echo

	@ echo DOWNLOADING ACCESSION TO FILE: LONG OPTION
	@ rm -f tmp/2
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 --output-file tmp/2 > /dev/null
#	@ echo

	@ echo DOWNLOADING ACCESSIONS
	@ rm tmp/sra/*
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 SRR045450 > /dev/null
#	@ echo

	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	  if $(BINDIR)/prefetch SRR045450 SRR053325 -o tmp/o 2> /dev/null ; \
	  then echo unexpected success when downloading multiple items to file ; \
	      exit 1 ; \
	  fi ;

	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	  if $(BINDIR)/prefetch SRR045450 SRR053325 -output-file tmp/o \
                                                                2> /dev/null ; \
	  then echo unexpected success when downloading multiple items to file ; \
	      exit 1 ; \
	  fi ;

	@ echo  version 1.0     >  tmp/k
	@ echo '0||SRR045450||' >> tmp/k
	@ echo '0||SRR053325||' >> tmp/k
	@ echo '$$end'          >> tmp/k

	@ if [ "$(BUILD)" = "dbg" ]; then \
	      echo DOWNLOADING TEXT KART ; \
	      rm tmp/sra/* ; \
	      export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch --text-kart tmp/k > /dev/null ; \
          \
	      echo DOWNLOADING TEXT KART: ORDER BY SIZE / SHORT OPTION ; \
	      rm tmp/sra/* ; \
	      export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch --text-kart tmp/k -os > /dev/null ; \
          \
	      echo DOWNLOADING TEXT KART: ORDER BY KART / SHORT OPTION ; \
	      rm tmp/sra/* ; \
	      export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch --text-kart tmp/k -ok > /dev/null ; \
          \
	      echo DOWNLOADING TEXT KART: ORDER BY SIZE / LONG OPTION ; \
	      rm tmp/sra/* ; \
	      export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch --text-kart tmp/k --order s > /dev/null ; \
          \
	      echo DOWNLOADING TEXT KART: ORDER BY KART / LONG OPTION ; \
	      rm tmp/sra/* ; \
	      export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch --text-kart tmp/k --order k > /dev/null ; \
	      fi

	@ rm -r tmp*
