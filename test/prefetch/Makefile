default: runtests

TOP ?= $(abspath ../..)

include $(TOP)/build/Makefile.env

CWD = $(shell pwd)
PUBLIC=/repository/user/main/public
RUN=https://sra-download.ncbi.nlm.nih.gov/traces/sra6/SRR/000052/SRR053325

runtests: check_prefetch

################################################################################
check_prefetch:
	@mkdir -pv tmp
	@echo
	echo 'foo = "bar"' > tmp/t.kfg

	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    if $(BINDIR)/prefetch $(RUN); then exit 1; \
	    else echo "^^^ failure when user repository is not set is expected"; \
	    echo; fi

	echo '$(PUBLIC)/apps/sra/volumes/sraFlat = "sra"' >  tmp/t.kfg
	echo '$(PUBLIC)/root              = "$(CWD)/tmp"' >> tmp/t.kfg
	@echo SRR download when user repository is configured
	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; \
	    $(BINDIR)/prefetch $(RUN)

#	export VDB_CONFIG=`pwd`/tmp; export NCBI_SETTINGS=/ ; $(BINDIR)/vdb-config -on

	@echo
	@rm -rv tmp
