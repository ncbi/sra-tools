default: runtests
TOP ?= $(abspath ../..)
include $(TOP)/build/Makefile.env
CWD = $(shell pwd)
PUBLIC=/repository/user/main/public
CGI=https://www.ncbi.nlm.nih.gov/Traces/names/names.cgi
LARGE=fasp://aspera@demo.asperasoft.com:aspera-test-dir-large/100MB
SRA=https://sra-download.ncbi.nlm.nih.gov
SRAF=fasp://dbtest@sra-download.ncbi.nlm.nih.gov
KMER=$(SRA)/traces/nannot01/kmer/000/390/GCA_000390265.1_R
KMERF=$(SRAF):data/sracloud/traces/nannot01/kmer/000/390/GCA_000390265.1_R
REFSEQ=$(SRA)/traces/refseq/KC702174.1
REFSEQF=$(SRAF):data/sracloud/traces/refseq/KC702174.1
SRR=$(SRA)/traces/sra6/SRR/000052/SRR053325
SRRF=$(SRAF):data/sracloud/traces/sra6/SRR/000052/SRR053325
WGS=$(SRA)/traces/wgs03/WGS/AF/VF/AFVF01.1
WGSF=$(SRAF):data/sracloud/traces/wgs03/WGS/AF/VF/AFVF01.1

runtests: urls_and_accs out_dir_and_file

################################################################################
urls_and_accs:
	@mkdir -pv tmp tmp2
	@rm -frv tmp/* tmp2/*
	echo 'foo = "bar"' > tmp/t.kfg

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch http://intranet/; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch http://intranet/irb/support.shtml; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/file/volumes/flat = "files"' > tmp/t.kfg
	echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg

	@if ls `pwd`/tmp2/support.shtml ; then exit 1; fi
	@echo HTTP download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch http://intranet/irb/support.shtml
	@ls `pwd`/tmp2/support.shtml
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch http://intranet/irb/support.shtml |grep "found local"
	@echo

	@if ls `pwd`/tmp2/index.html ; then exit 1; fi
	@echo HTTP download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch http://intranet/ > /dev/null
	@ls `pwd`/tmp2/index.html
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch http://intranet/ | grep "found local"
	@rm -v tmp2/index.html
	@echo

	@echo URL download when user repository is configured
	@if ls `pwd`/tmp2/wiki ; then exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@ls `pwd`/tmp2/wiki
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki \
	                              | grep "found local" > /dev/null
	@echo

	@echo URL/ download when user repository is configured
	@if ls `pwd`/tmp2/index.html ; then exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ > /dev/null
	@ls `pwd`/tmp2/index.html
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/|grep "found local">/dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(SRR) 2> /dev/null ; then exit 1; \
	    else echo "Failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg
	@if ls `pwd`/tmp/sra/SRR053325.sra ; then exit 1; fi
	@echo SRR download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@ls `pwd`/tmp/sra/SRR053325.sra
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(REFSEQ) 2> /dev/null ; then exit 1; \
	    else echo "Failure when user repository is incomplete is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/refseq/volumes/refseq = "refseq"' >> tmp/t.kfg
	@echo REFSEQ download when user repository is configured
	@if ls `pwd`/tmp/refseq/KC702174.1 ; then exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) > /dev/null
	@ls `pwd`/tmp/refseq/KC702174.1
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(KMER) 2> /dev/null ; then exit 1; \
	    else echo "Failure when user repository is incomplete is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/nakmer/volumes/nakmerFlat = "nannot"' >> tmp/t.kfg
	@echo NANNOT download when user repository is configured
	@if ls `pwd`/tmp/nannot/GCA_000390265.1_R ; then exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) > /dev/null
	@ls `pwd`/tmp/nannot/GCA_000390265.1_R
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) | grep "found local" > /dev/null

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(WGS) 2> /dev/null; then exit 1; \
	    else echo "Failure when user repository is incomplete is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/t.kfg
	@echo WGS download when user repository is configured
	@if ls `pwd`/tmp/wgs/AFVF01 ; then exit 1; fi
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(WGS) > /dev/null
	@echo INCORRECT DOWNLOAD OF WGS SHOULD BE FIXED: VDB-3537
	@ls `pwd`/tmp/wgs/AFVF01
	@echo

	@rm `pwd`/tmp/sra/SRR053325.sra
	@echo SRR FASP download when user repository is configured
	if ascp -h > /dev/null ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) ; \
	    ls `pwd`/tmp/sra/SRR053325.sra ; \
	else echo download of $(SRRF) when ascp is not found is disabled ; \
	fi
	@echo

	@rm `pwd`/tmp/refseq/KC702174.1
	@echo REFSEQ download when user repository is configured
	@if ascp -h > /dev/null ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(REFSEQF) ; \
	    ls `pwd`/tmp/refseq/KC702174.1 ; \
	else echo download of $(REFSEQF) when ascp is not found is disabled ; \
	fi
	@echo

	@rm `pwd`/tmp/nannot/GCA_000390265.1_R
	@echo NANNOT FASP download when user repository is configured
	@if ascp -h > /dev/null ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(KMERF) ; \
	      ls `pwd`/tmp/nannot/GCA_000390265.1_R ; \
	else echo download of $(KMERF) when ascp is not found is disabled ; \
	fi
	@echo

	@rm `pwd`/tmp/wgs/AFVF01
	@echo WGS download when user repository is configured
	@if ls `pwd`/tmp/wgs/AFVF01 ; then exit 1; fi
	@if ascp -h > /dev/null ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	      $(BINDIR)/prefetch $(WGSF) ; \
	    echo INCORRECT DOWNLOAD OF WGS SHOULD BE FIXED: VDB-3537 ; \
	    ls `pwd`/tmp/wgs/AFVF01 ; \
	else echo download of $(WGSF) when ascp is not found is disabled ; \
	fi
	@echo

	@rm -vf `pwd`/tmp/sra/SRR053325.sra
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch SRR053325 > /dev/null ; then exit 1; \
	    else echo "Failure when remote repository is not set is expected"; \
	    echo; fi

	echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' >> tmp/t.kfg
	@echo SRR accession download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 > /dev/null
	@ls `pwd`/tmp/sra/SRR053325.sra
	@echo

	@echo REFSEQ accession download
	@rm -vf `pwd`/tmp/refseq/KC702174.1
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch KC702174.1 > /dev/null
	@ls `pwd`/tmp/refseq/KC702174.1
	@echo

	@echo NANNOT accession download
	@rm -vf `pwd`/tmp/nannot/GCA_000390265.1_R
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch GCA_000390265.1_R
	@ls `pwd`/tmp/nannot/GCA_000390265.1_R
	@echo

	@echo WGS accession download
	@rm -vf `pwd`/tmp/wgs/AFVF01
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch AFVF01
	@ls `pwd`/tmp/wgs/AFVF01
	@echo

	@echo FASP download: asperasoft/100MB
	@if ls `pwd`/tmp2/100MB ; then exit 1; fi
	if ascp -h > /dev/null ; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch $(LARGE) ; \
	    ls `pwd`/100MB ; \
	else \
	  if $(BINDIR)/prefetch $(LARGE) ; then exit 1; \
	  else \
	   echo prefetch failure for FASP URL when ascp is not found is expected ; \
	  fi ; \
	fi
	@echo

	@echo FASP download: ncbi/1GB
	@if ls `pwd`/tmp2/1GB ; then exit 1; fi
	if ascp -h > /dev/null ; then \
	  export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:1GB ; \
	    ls `pwd`/1GB ; \
	fi
	@echo

	@rm -vf `pwd`/tmp2/100MB
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if $(BINDIR)/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:100MB; \
	    then exit 1; else echo "Expected failure when FASP source not found"; \
	    echo; fi

#	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; $(BINDIR)/vdb-config -on

	@rm -rv tmp*

out_dir_and_file:
	@ echo
	@ mkdir -pv tmp tmp2
	@ rm -frv tmp*/*
	@ echo  version 1.0     >  tmp/k
	@ echo '0||SRR053325||' >> tmp/k
	@ echo '$$end'          >> tmp/k
	@ echo
	@ echo 'repository/remote/main/CGI/resolver-cgi = "$(CGI)"' >  tmp/t.kfg
	@ echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"'         >> tmp/t.kfg
	@ echo '$(PUBLIC)/root = "$(CWD)/tmp"'                      >> tmp/t.kfg

################################################################################
################################################################################

	@ if [ "$(BUILD)" = "dbg" ]; then \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch --text-kart tmp/k -O tmp/o ; then exit 1 ; \
	    else echo "FAILURE WHEN PREFETCH TEXT-KART TO OUT-FILE IS EXPECTED" ; \
	    echo; fi ; \
	    \
	    echo PREFETCH TEXT-KART ; \
	    rm -v `pwd`/tmp/sra/SRR053325.sra ; \
	    $(BINDIR)/prefetch --text-kart tmp/k > /dev/null ; \
	    ls `pwd`/tmp/sra/SRR053325.sra ; \
	    echo ; \
	fi

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	 if $(BINDIR)/prefetch SRR045450 SRR053325 -O tmp/o ; then exit 1 ; \
	 else echo FAILURE WHEN PREFETCH MULTIPLE ITEMS TO OUT-FILE IS EXPECTED ; \
	 echo; fi ;

	@ echo PREFETCH MULTIPLE ITEMS
	@ if ls `pwd`/tmp/sra/SRR045450.sra ; then exit 1; fi ;
	@ rm -v `pwd`/tmp/sra/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR045450 SRR053325 > /dev/null
	@ ls `pwd`/tmp/sra/SRR045450.sra `pwd`/tmp/sra/SRR053325.sra
	@ echo

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	 if $(BINDIR)/prefetch -O tmp/o kart.krt ; then exit 1 ; \
	 else echo FAILURE WHEN PREFETCH KART TO OUT-FILE IS EXPECTED ; \
	 echo; fi ;

	@ echo PREFETCH SRR HTTP URL
	@ rm -v `pwd`/tmp/sra/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@ ls `pwd`/tmp/sra/SRR053325.sra
	@ echo

	@if ascp -h > /dev/null ; then \
	    echo PREFETCH SRR FASP URL ; \
	    rm -v `pwd`/tmp/sra/SRR053325.sra ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) > /dev/null ; \
	    ls `pwd`/tmp/sra/SRR053325.sra ; \
	else echo download of SRR FASP URL when ascp is not found is disabled ; \
	fi
	@ echo

	@ echo PREFETCH HTTP DIRECTORY URL
	@ if ls `pwd`/tmp2/index.html ; then exit 1; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ > /dev/null
	@ ls `pwd`/tmp2/index.html
	@ echo

	@ echo PREFETCH HTTP FILE URL
	@ if ls `pwd`/tmp2/wiki ; then exit 1; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@ ls `pwd`/tmp2/wiki
	@ echo

	@if ascp -h > /dev/null ; then \
	    echo PREFETCH FASP URL ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    if ls `pwd`/100MB ; then exit 1; fi ; \
	    $(BINDIR)/prefetch $(LARGE) > /dev/null ; \
	    ls `pwd`/100MB ; \
	 else echo download of SRR FASP URL when ascp is not found is disabled ; \
	 fi
	@ echo

	@ echo PREFETCH ACCESSION TO OUT-DIR
	@ rm -fv `pwd`/tmp3/dir/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch SRR053325 -P tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/SRR053325.sra
	@ echo

	@ echo PREFETCH SRR HTTP URL TO OUT-DIR
	@ rm -fv `pwd`/tmp3/dir/SRR053325.sra
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) -P tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/SRR053325.sra
	@ echo

	@if ascp -h > /dev/null ; then \
	    echo PREFETCH SRR FASP URL TO OUT-DIR ; \
	    rm -fv `pwd`/tmp3/dir/SRR053325.sra ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRRF) -P tmp3/dir > /dev/null ; \
	    ls `pwd`/tmp3/dir/SRR053325.sra ; \
	else echo download of SRR FASP URL when ascp is not found is disabled ; \
	fi
	@ echo

	@ echo PREFETCH HTTP DIRECTORY URL TO OUT-DIR
	@ if ls `pwd`/tmp3/dir/index.html ; then exit 1; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ -P tmp3/dir > /dev/null
	@ ls `pwd`/tmp3/dir/index.html
	@ echo

	@ echo PREFETCH HTTP FILE URL TO OUT-DIR
	@ if ls `pwd`/tmp3/dir/wiki ; then exit 1; fi
	@ export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki -P tmp3/dir \
	                       > /dev/null
	@ ls `pwd`/tmp3/dir/wiki
	@ echo

	@if ascp -h > /dev/null ; then \
	    echo PREFETCH FASP URL TO OUT-DIR ; \
	    export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if `pwd`/tmp3/dir/100MB ; then exit 1; fi ; \
	    $(BINDIR)/prefetch $(LARGE)  -P tmp3/dir > /dev/null ; \
	    ls `pwd`/tmp3/dir/100MB ; \
	 else echo download of SRR FASP URL when ascp is not found is disabled ; \
	 fi
	@ echo

	@ rm -r tmp*
