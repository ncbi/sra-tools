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

MODULE = test/general-loader

TEST_TOOLS = \
    test-general-loader

ALL_TOOLS = \
	$(TEST_TOOLS) \
	test-general-writer

include $(TOP)/build/Makefile.env

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# test-general-loader
#
TEST_GEN_LOAD_SRC = \
	test-general-loader

TEST_GEN_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_GEN_LOAD_SRC))

TEST_GEN_LOAD_LIB =   \
	-sncbi-wvdb-static   \
	-skapp              \
    -sktst              \

$(TEST_BINDIR)/test-general-loader: $(TEST_GEN_LOAD_OBJ)
	$(LP) --exe -o $@ $^ $(TEST_GEN_LOAD_LIB)

gen_load: test-general-loader
	$(TEST_BINDIR)/test-general-loader
    
vg_gen_load: test-general-loader
	valgrind --ncbi $(TEST_BINDIR)/test-general-loader

#-------------------------------------------------------------------------------
# test-general-writer
#
TEST_GEN_WRITE_SRC = \
	test-general-writer

TEST_GEN_WRITE_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_GEN_WRITE_SRC))

TEST_GEN_WRITE_LIB =   \
    -L$(LIBDIR) \
    -sgeneral-writer


$(TEST_BINDIR)/test-general-writer: $(TEST_GEN_WRITE_OBJ)
	$(LP) --exe -o $@ $^ $(TEST_GEN_WRITE_LIB)

vg_gen_write: test-general-writer
	valgrind --ncbi $(TEST_BINDIR)/test-general-writer -o gen-write-out
	rm -f gen-write-out


runtests: test-general-writer piped-load-tests

piped-load-tests: $(ALL_TOOLS)
	$(TEST_BINDIR)/test-general-writer | $(BINDIR)/general-loader
	$(BINDIR)/vdb-dump -T table1 remote_db
	rm -rf remote_db

#-------------------------------------------------------------------------------
# general-loader tool tests
#
#runtests: tool-tests

makeinputs: makeinputs.cpp
	# generate numbered input streams for tool tests
	$(CXX) makeinputs.cpp -o makeinputs -I$(INCDIRS)

tool-tests: makeinputs
	mkdir -p inputs && ./makeinputs
	$(SRCDIR)/runtestcase.sh $(BINDIR) 1 