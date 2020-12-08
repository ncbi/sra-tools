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

MODULE = test/vdb-dump

TEST_TOOLS =

include $(TOP)/build/Makefile.env

CONFIGTOUSE ?= NCBI_SETTINGS
DIRTOTEST ?= $(BINDIR)

EXT_TOOLS = \
	vdb-dump-makedb

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# all
#
$(TARGDIR)/all: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/all

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(EXT_TOOLS))

.PHONY: $(TEST_TOOLS)

runtests: announce vdb-dump testing_returncode

announce:
	@echo Testing $(DIRTOTEST) CONFIGTOUSE=$(CONFIGTOUSE)

testing_returncode:
	@echo Testing returncode...
	@$(BINDIR)/vdb-dump ./VDB-3937.kar -R1 -C READ > /dev/null
	@! $(BINDIR)/vdb-dump ./VDB-3937.kar -L sys -R1 -C READ,BLAH > /dev/null
	@echo Testing returncode: success

#-------------------------------------------------------------------------------
# vdb-dump-makedb
# Create test databases
MAKEDB_SRC = \
	makedb

MAKEDB_OBJ = \
	$(addsuffix .$(OBJX),$(MAKEDB_SRC))

MAKEDB_LIB = \
	-skapp \
	-sktst \
	-sncbi-wvdb \

$(BINDIR)/vdb-dump-makedb: $(MAKEDB_OBJ)
	$(LP) --exe -o $@ $^ $(MAKEDB_LIB)

makedb:
	@ cd $(SRCDIR); rm -rf data; mkdir -p data; $(BINDIR)/vdb-dump-makedb

#-------------------------------------------------------------------------------
# run tests for vdb-dump
ifdef PYTHON
vdb-dump: makedb
	@ rm -rf actual
	@ mkdir -p actual
	@ echo "testing vdb-dump..."
	@ # output format
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump SRR056386 -R 1 -C READ -f tab \
	    >actual/1.0.stdout && diff expected/1.0.stdout actual/1.0.stdout
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump SRR056386 -R 1 -C READ -f tab -I \
	    >actual/1.1.stdout && diff expected/1.1.stdout actual/1.1.stdout
	@ # nested databases
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump -E data/NestedDatabase \
	    >actual/2.0.stdout && diff expected/2.0.stdout actual/2.0.stdout
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump -T SUBDB_1.SUBSUBDB_1.TABLE1 \
	       data/NestedDatabase >actual/2.1.stdout \
	    && diff expected/2.1.stdout actual/2.1.stdout
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump -T SUBDB_1.SUBSUBDB_2.TABLE2 \
	       data/NestedDatabase >actual/2.2.stdout \
	    && diff expected/2.2.stdout actual/2.2.stdout
	@ rm -rf actual
	@ rm -rf data
	@ $(PYTHON) $(TOP)/build/check-exit-code.py $(DIRTOTEST)/vdb-dump
	@ ./test_buffer_insufficient.sh $(DIRTOTEST)/vdb-dump VDB-3937.kar
	@ echo "...all tests passed"

else
vdb-dump: makedb
	@ rm -rf actual
	@ mkdir -p actual
	@ echo "testing vdb-dump..."
	@ # output format
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump SRR056386 -R 1 -C READ -f tab \
	    >actual/1.0.stdout && diff expected/1.0.stdout actual/1.0.stdout
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump SRR056386 -R 1 -C READ -f tab -I \
	    >actual/1.1.stdout && diff expected/1.1.stdout actual/1.1.stdout
	@ # nested databases
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump -E data/NestedDatabase >actual/2.0.stdout && diff expected/2.0.stdout actual/2.0.stdout
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump -T SUBDB_1.SUBSUBDB_1.TABLE1 \
	       data/NestedDatabase >actual/2.1.stdout \
	    && diff expected/2.1.stdout actual/2.1.stdout
	@ $(CONFIGTOUSE)=/ $(DIRTOTEST)/vdb-dump -T SUBDB_1.SUBSUBDB_2.TABLE2 \
	       data/NestedDatabase >actual/2.2.stdout \
	    && diff expected/2.2.stdout actual/2.2.stdout
	@ rm -rf actual
	@ rm -rf data
	@ ./test_buffer_insufficient.sh $(DIRTOTEST)/vdb-dump VDB-3937.kar
	@ echo "...all tests passed"

endif

.PHONY: kar

