default: runtests
TOP ?= $(abspath ../..)
include $(TOP)/build/Makefile.env
CWD = $(shell pwd)
PUBLIC=/repository/user/main/public
SRA=https://sra-download.ncbi.nlm.nih.gov
KMER=$(SRA)/traces/nannot01/kmer/000/390/GCA_000390265.1_R
REFSEQ=$(SRA)/traces/refseq/KC702174.1
SRR=$(SRA)/traces/sra6/SRR/000052/SRR053325
WGS=$(SRA)/traces/wgs03/WGS/AF/VF/AFVF01.1

runtests: check_prefetch

################################################################################
check_prefetch:
	@mkdir -pv tmp tmp2
	echo 'foo = "bar"' > tmp/t.kfg

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' > tmp/t.kfg

	@echo URL download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki > /dev/null
	@echo

	@#echo URL/ download when user repository is configured
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/
	@echo

	@echo Running prefetch second time finds previous download
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ | grep "found local"
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki | grep "found local"

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(SRR); then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg
	@echo SRR download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) > /dev/null
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(SRR) | grep "found local"

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(REFSEQ); then exit 1; \
	    else echo "^^^ failure when user repository is incomplete is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/refseq/volumes/refseq = "refseq"' >> tmp/t.kfg
	@echo REFSEQ download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) > /dev/null
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(REFSEQ) | grep "found local"

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(KMER) 2> /dev/null ; then exit 1; \
	    else echo "Failure when user repository is incomplete is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/nakmer/volumes/nakmerFlat = "nannot"' >> tmp/t.kfg
	@echo NANNOT download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) > /dev/null
	@echo

	@echo Running prefetch second time finds previous download
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(KMER) | grep "found local"

	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(WGS) 2> /dev/null; then exit 1; \
	    else echo "Failure when user repository is incomplete is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/t.kfg
	@echo WGS download when user repository is configured
	@export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(WGS) > /dev/null
	@echo

#	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; $(BINDIR)/vdb-config -on

	@rm -rv tmp*
