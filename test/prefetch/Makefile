default: runtests

TOP ?= $(abspath ../..)

include $(TOP)/build/Makefile.env

CWD = $(shell pwd)
PUBLIC=/repository/user/main/public
RUN=https://sra-download.ncbi.nlm.nih.gov/traces/sra6/SRR/000052/SRR053325

runtests: check_prefetch

################################################################################
check_prefetch:
	@mkdir -pv tmp tmp2
	echo 'foo = "bar"' > tmp/t.kfg

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki; then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' > tmp/t.kfg

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki
	@echo

	@echo Running prefetch second time finds previous download
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; cd tmp2 ; \
	    $(BINDIR)/prefetch https://github.com/ncbi/ngs/wiki | grep "found local"


	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(RUN); then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/root = "$(CWD)/tmp"' >> tmp/t.kfg
	@echo SRR download when user repository is configured
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(RUN)
	@echo

	@echo Running prefetch second time finds previous download
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(RUN) | grep "found local"

#	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; $(BINDIR)/vdb-config -on

	@echo
	@rm -rv tmp*
