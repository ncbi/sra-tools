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


default: std

TOP ?= $(abspath ../..)
MODULE = tools/sra-sort

include $(TOP)/build/Makefile.shell

INT_TOOLS = \
	dump-blob-boundaries

EXT_TOOLS = \

ifneq (win,$(OS))
EXT_TOOLS += sra-sort
endif	

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env
include $(SRCDIR)/Makefile.$(COMP)

#------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# sra-sort
#
SRA_SORT_SRC = \
	caps \
	mem \
	membank \
	paged-membank \
	paged-mmapbank \
	except \
	idx-mapping \
	map-file \
	col-pair \
	row-set \
	simple-row-set \
	mapping-row-set \
	sorting-row-set \
	meta-pair \
	dir-pair \
	tbl-pair \
	db-pair \
	glob-poslen \
	poslen-col-pair \
	ref-alignid-col \
	buff-writer \
	id-mapper-col \
	capture-first-half-aligned \
	csra-tbl \
	csra-pair \
	run \
	sra-sort \
	xcheck-ref-align

SRA_SORT_OBJ = \
	$(addsuffix .$(OBJX),$(SRA_SORT_SRC))

SRA_SORT_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/sra-sort: $(SRA_SORT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(SRA_SORT_LIB)

#-------------------------------------------------------------------------------
# dump-blob-boundaries
#
DBB_SRC = \
	dump-blob-boundaries

DBB_OBJ = \
	$(addsuffix .$(OBJX),$(DBB_SRC))

DBB_LIB = \
	-lncbi-vdb \

$(BINDIR)/dump-blob-boundaries: $(DBB_OBJ)
	$(LD) --exe -o $@ $^ $(DBB_LIB)
