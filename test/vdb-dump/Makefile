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

TEST_TOOLS = \
	test-view-spec

include $(TOP)/build/Makefile.env

EXT_TOOLS = \
	vdb-dump-makedb

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

clean: stdclean

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@
	@ mkdir -p actual db

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

runtests: vdb-dump

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
# vdb-view-spec
# vcommand line view spec parsing
VIEW_SPEC_SRC = \
	test-view-spec \

VIEW_SPEC_OBJ = \
	$(addsuffix .$(OBJX),$(VIEW_SPEC_SRC))

VIEW_SPEC_LIB = \
	-skapp \
	-sktst \
	-sncbi-wvdb \

$(TEST_BINDIR)/test-view-spec: $(VIEW_SPEC_OBJ)
	$(LP) --exe -o $@ $^ $(VIEW_SPEC_LIB)

viewspec: test-view-spec
	$(TEST_BINDIR)/test-view-spec

#-------------------------------------------------------------------------------
# run tests for vdb-dump
VDB_DUMP = $(BINDIR)/vdb-dump
vdb-dump: makedb
	@ rm -rf actual
	@ mkdir -p actual
	@ echo "testing vdb-dump..."
	# output format
	@ $(VDB_DUMP) SRR056386 -R 1 -C READ -f tab >actual/1.0.stdout && diff expected/1.0.stdout actual/1.0.stdout
	@ $(VDB_DUMP) SRR056386 -R 1 -C READ -f tab -I >actual/1.1.stdout && diff expected/1.1.stdout actual/1.1.stdout
	# nested databases
	@ $(VDB_DUMP) -E data/NestedDatabase >actual/2.0.stdout && diff expected/2.0.stdout actual/2.0.stdout
	@ $(VDB_DUMP) -T SUBDB_1.SUBSUBDB_1.TABLE1 data/NestedDatabase >actual/2.1.stdout && diff expected/2.1.stdout actual/2.1.stdout
	#
	# Views
	#	the accessions used below were taken from a blackbox test run, each represents a
	#  	different table type in a CSRA object.
	#
	# NCBI:align:tbl:seq; 2 views (used to clash on Id with tables)
	$(VDB_DUMP) SRR1063272 -S view.vschema --view "V1<SEQUENCE>" -R 1-2 >actual/3.0.stdout && diff expected/3.0.stdout actual/3.0.stdout
	$(VDB_DUMP) SRR1063272 -S view.vschema --view "V2<SEQUENCE>" -R 1-2 >actual/3.1.stdout && diff expected/3.1.stdout actual/3.1.stdout
	# NCBI:SRA:Nanopore:sequence
	$(VDB_DUMP) ERR968961 -S view.nanopore.vschema --view "V<SEQUENCE>" -R 1-2 >actual/3.2.stdout && diff expected/3.2.stdout actual/3.2.stdout
	# NCBI:SRA:PacBio:smrt:sequence (very old and the latest)
	$(VDB_DUMP) SRR353493 -S view.pacbio.vschema --view "V<SEQUENCE>" -R 1-2 >actual/3.3.stdout && diff expected/3.3.stdout actual/3.3.stdout
	$(VDB_DUMP) ERR976792 -S view.pacbio.vschema --view "V<SEQUENCE>" -R 1-2 >actual/3.4.stdout && diff expected/3.4.stdout actual/3.4.stdout
	# NCBI:align:db:alignment_evidence_sorted
	$(VDB_DUMP) SRR2070515 -S view.vschema --view "V1<SEQUENCE>" -R 1-2 >actual/3.5.stdout && diff expected/3.5.stdout actual/3.5.stdout
	# NCBI:align:db:alignment_sorted
	$(VDB_DUMP) ERR957001 -S view.vschema --view "V1<SEQUENCE>" -R 1-2 >actual/3.6.stdout && diff expected/3.6.stdout actual/3.6.stdout
	# NCBI:align:db:alignment_unsorted. this will issue some "Undeclared identifier" messages for views that rely on table types not in this accession's schema, but will do the job since V1's declaration is correct
	$(VDB_DUMP) ERR570054 -S view.vschema --view "V1<SEQUENCE>" -R 1-2 >actual/3.7.stdout && diff expected/3.7.stdout actual/3.7.stdout
	# NULL as foreign key
	$(VDB_DUMP) SRR1063272 --view "V4<PRIMARY_ALIGNMENT,SEQUENCE>" -S view.vschema  -R 6  >actual/3.8.stdout && diff expected/3.8.stdout actual/3.8.stdout
	# join a table to itself
	$(VDB_DUMP) SRR1063272 --view "V6<PRIMARY_ALIGNMENT,SEQUENCE>" -S view.vschema  -R 36  >actual/3.9.stdout && diff expected/3.9.stdout actual/3.9.stdout
	# view on view
	$(VDB_DUMP) SRR1063272 --view "V7<V6<PRIMARY_ALIGNMENT,SEQUENCE>>" -S view.vschema  -R 36  >actual/3.10.stdout && diff expected/3.10.stdout actual/3.10.stdout
	# table object - views not supported
	! $(VDB_DUMP) ERR997444 -S view.vschema --view "V1<SEQUENCE>"

	@ rm -rf actual
	@ rm -rf data
	@ python $(TOP)/build/check-exit-code.py $(BINDIR)/vdb-dump
	@ echo "...all tests passed"

onetest: std
	#$(VDB_DUMP) ERR570054 -S view.vschema --view "V1<SEQUENCE>" -R 1-2 >actual/3.7.stdout && diff expected/3.7.stdout actual/3.7.stdout
	# row not found
	$(VDB_DUMP) SRR1063272 --view "V7<V6<PRIMARY_ALIGNMENT,SEQUENCE>>" -S view.vschema  -R 36  >actual/3.10.stdout

ACC = SRR2070515
acc:
	$(VDB_DUMP) $(ACC) -T SEQUENCE -C READ -R 1-2 >actual/$(ACC).old
	cat actual/$(ACC).old
	$(VDB_DUMP) $(ACC) -S view.vschema --view "V1<SEQUENCE>" -R 1-2 >actual/$(ACC).new
	diff actual/$(ACC).old actual/$(ACC).new


.PHONY: kar

