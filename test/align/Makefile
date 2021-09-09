# ===========================================================================
#
#							PUBLIC DOMAIN NOTICE
#			   National Center for Biotechnology Information
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
MODULE = test/align

include $(TOP)/build/Makefile.env

# default: runtests

# TOP ?= $(abspath ../..)
# MODULE = test/align

# TEST_TOOLS = \
	# test-load-index \
	# test-samextract \
	# test-estimator

# include $(TOP)/build/Makefile.env

# $(TEST_TOOLS): makedirs
	# @ $(MAKE_CMD) $(TEST_BINDIR)/$@

# clean: stdclean

# INCDIRS += -I$(TOP)/libs/align -I$(TOP)/../ngs-tools/libs/general-writer/inc

# #-------------------------------------------------------------------------------
# # test-load-index
# #
# TEST_INDEX_SRC = \
	# indextest

# TEST_INDEX_OBJ = \
	# $(addsuffix .$(OBJX),$(TEST_INDEX_SRC))

# TEST_INDEX_LIB = \
	# -skapp \
	# -sktst \
	# -sncbi-vdb 

# $(TEST_BINDIR)/test-load-index: $(TEST_INDEX_OBJ)
	# $(LP) --exe -o $@ $^ $(TEST_INDEX_LIB)

# #-------------------------------------------------------------------------------
# # test-estimator
# #
# TEST_ESTIMATOR_SRC = \
	# pileup_estimator

# TEST_ESTIMATOR_OBJ = \
	# $(addsuffix .$(OBJX),$(TEST_ESTIMATOR_SRC))

# TEST_ESTIMATOR_LIB = \
	# -skapp \
	# -sktst \
	# -sncbi-ngs-c++ \
	# -sncbi-ngs \
	# -sncbi-vdb \
	# -L$(NGS_LIBDIR) \
	# -sngs-c++

# $(TEST_BINDIR)/test-estimator: $(TEST_ESTIMATOR_OBJ)
	# $(LP) --exe -o $@ $^ $(TEST_ESTIMATOR_LIB)

# #-------------------------------------------------------------------------------
# # samtogw
# #
# SAMTOGW_SRC = \
		# samtogw

# SAMTOGW_OBJ = \
		# $(addsuffix .$(OBJX),$(SAMTOGW_SRC))

# SAMTOGW_LIB = \
		# -L/home/vartanianmh/ncbi-outdir/ngs-tools/linux/gcc/x86_64/dbg/ilib/ \
		# -skapp \
		# -sktst \
		# -ssam-extract\
		# -sncbi-vdb \
		# -sgeneral-writer

# $(TEST_BINDIR)/samtogw: $(SAMTOGW_OBJ)
		# $(LP) --exe -o $@ $^ $(SAMTOGW_LIB)

# #-------------------------------------------------------------------------------
# # bamfuzz
# #
# BAMFUZZ_SRC = \
		# bamfuzz

# BAMFUZZ_OBJ = \
		# $(addsuffix .$(OBJX),$(BAMFUZZ_SRC))

# BAMFUZZ_LIB = \
		# -skapp \
		# -sktst \
		# -ssam-extract\
		# -sncbi-vdb

# $(TEST_BINDIR)/bamfuzz: $(BAMFUZZ_OBJ)
		# $(LP) --exe -o $@ $^ $(BAMFUZZ_LIB)

# #-------------------------------------------------------------------------------
# # irvrfy
# #
# IRVRFY_SRC = \
		# irvrfy

# IRVRFY_OBJ = \
		# $(addsuffix .$(OBJX),$(IRVRFY_SRC))

# IRVRFY_LIB = \
		# -L/home/vartanianmh/ncbi-outdir/ngs-tools/linux/gcc/x86_64/dbg/ilib/ \
		# -skapp \
		# -sktst \
		# -ssam-extract\
		# -sncbi-wvdb \
		# -sgeneral-writer

# $(TEST_BINDIR)/irvrfy: $(IRVRFY_OBJ)
		# $(LP) --exe -o $@ $^ $(IRVRFY_LIB)


# #-------------------------------------------------------------------------------
# # test-samextract
# #
# TEST_SAMEXTRACT_SRC = \
		# test-samextract

# TEST_SAMEXTRACT_OBJ = \
		# $(addsuffix .$(OBJX),$(TEST_SAMEXTRACT_SRC))

# TEST_SAMEXTRACT_LIB = \
		# -skapp \
		# -ssam-extract\
		# -sktst \
		# -sncbi-vdb

# $(TEST_BINDIR)/test-samextract: $(TEST_SAMEXTRACT_OBJ)
		# $(LP) --exe -o $@ $^ $(TEST_SAMEXTRACT_LIB)

# valgrind_samextract: std
	# valgrind --ncbi --show-reachable=no $(TEST_BINDIR)/test-samextract

# run-test-samextract: test-samextract
		# @ export LD_LIBRARY_PATH=$(LIBDIR) ; $(TEST_BINDIR)/test-samextract


# # runtests
# #runtests: run-test-samextract run-test-load-index
# runtests: run-test-samextract

# miketoys: $(TEST_BINDIR)/irvrfy $(TEST_BINDIR)/samtogw $(TEST_BINDIR)/bamfuzz

# .PHONY: $(TEST_TOOLS)


