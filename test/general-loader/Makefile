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
    test-general-loader \
	test-general-writer \
	test-gw-dumper

ALL_TOOLS = \
	$(TEST_TOOLS) \

include $(TOP)/build/Makefile.env

INCDIRS += -I$(TOP)/tools/general-loader

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# test-general-loader
#
TEST_GEN_LOAD_SRC = \
	test-general-loader \
    testsource

TEST_GEN_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_GEN_LOAD_SRC))

TEST_GEN_LOAD_LIB =   \
	-sncbi-wvdb-static  \
    -sload              \
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


runtests:$(TEST_TOOLS) #piped-load-tests

piped-load-tests: $(ALL_TOOLS)
	-$(TEST_BINDIR)/test-general-writer | $(BINDIR)/general-loader -L=info
	-$(BINDIR)/vdb-dump -T table1 remote_db
	rm -rf remote_db

#-------------------------------------------------------------------------------
# test-gw-dumper
#
TEST_GW_DUMP_SRC = \
	test-gw-dumper

TEST_GW_DUMP_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_GW_DUMP_SRC))

TEST_GW_DUMP_LIB =   \
	-sgeneral-writer  \

$(TEST_BINDIR)/test-gw-dumper: $(TEST_GW_DUMP_OBJ)
	$(LP) --exe -o $@ $^ $(TEST_GW_DUMP_LIB)

gw_dump: test-gw-dumper
	$(TEST_BINDIR)/$^

vg_gw_dump: test-gw-dumper
	valgrind --ncbi $(TEST_BINDIR)/test-gw-dumper

#-------------------------------------------------------------------------------
# general-loader tool tests
#
runtests: setup tool-tests 

setup:
	@ mkdir -p db

$(TEST_BINDIR)/makeinputs: makeinputs.cpp
	@# generate numbered input streams for tool tests
	$(CXX) makeinputs.cpp testsource.cpp -o $(TEST_BINDIR)/makeinputs -I$(INCDIRS) -L$(VDB_LIBDIR) -lncbi-vdb-static

tool-tests: $(TEST_BINDIR)/makeinputs
	@mkdir -p input && $(TEST_BINDIR)/makeinputs && rm $(TEST_BINDIR)/makeinputs
	@#    
	@# basic, unpacked
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 1 0 -I=$(VDB_INCDIR)
	@#    
	@# basic, packed
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 1packed 0 -I=$(VDB_INCDIR)
	@#    
	@# target db override 
	@# use 1.gl which specifies the target as actual/1/db, and override that to where the script expects to see the result
	@cp input/1.gl input/1override.gl && $(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 1override 0 "-I=$(VDB_INCDIR) --target actual/1override/db"
	@# same with the storthand option
	@cp input/1.gl input/1override.gl && $(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 1override 0 "-I=$(VDB_INCDIR) -T actual/1override/db"
	@#    
	@# error message event, unpacked
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 2 3 "-I=$(VDB_INCDIR) -L=err"
	@#    
	@# error message event, packed
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 2packed 3 "-I=$(VDB_INCDIR) -L=err"
	@#    
	@# empty default values, unpacked
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 3 0 -I=$(VDB_INCDIR)
	@#    
	@# empty default values, packed
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 3packed 0 -I=$(VDB_INCDIR)
	@#    
	@# move ahead event, unpacked
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 4 0 -I=$(VDB_INCDIR)
	@#    
	@# move ahead event, packed
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 4packed 0 -I=$(VDB_INCDIR)
	@#    
	@# integer compression (packed only)
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 5packed 0 "-L=info -I=$(VDB_INCDIR)"
	@#    
	@# software name in the metadata, unpacked   
	@$(SRCDIR)/runtestcase.sh $(BINDIR) kdbmeta $(SRCDIR) 6 0 "-L=info -I=$(VDB_INCDIR)" SOFTWARE/formatter
	@#    
	@# software name in the metadata, packed   
	@$(SRCDIR)/runtestcase.sh $(BINDIR) kdbmeta $(SRCDIR) 6packed 0 "-L=info -I=$(VDB_INCDIR)" SOFTWARE/formatter
	@#    
	@# log message, unpacked
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 7 0 -I=$(VDB_INCDIR)
	@#    
	@# log message, packed
	@$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 7packed 0 -I=$(VDB_INCDIR)
	@#    
	@rm -rf input/*.gl actual/*

one-tool: $(TEST_BINDIR)/makeinputs
	@mkdir -p input && $(TEST_BINDIR)/makeinputs && rm $(TEST_BINDIR)/makeinputs
	$(SRCDIR)/runtestcase.sh $(BINDIR) vdb-dump $(SRCDIR) 7 0 "-L=info  -z actual/7.xml -I=$(VDB_INCDIR)"


#-------------------------------------------------------------------------------
# general-loader tool tests
#
slowtests: #piped-dump

piped-dump: $(BINDIR)/pileup-stats test-general-loader
	#$(BINDIR)/pileup-stats SRR1063272 | $(BINDIR)/general-loader #-L=info
	$(BINDIR)/pileup-stats SRR1232302 | $(BINDIR)/general-loader -L=info

