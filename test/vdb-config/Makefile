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
MODULE = test/vdb-config

EXT_TOOLS = \
	test-vdb-config-model

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env # BINDIR

DIRTOTEST ?= $(BINDIR)

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

#-------------------------------------------------------------------------------
# outer targets
#
all std: vers-includes
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(ALL_TOOLS): vers-includes
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

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# vers-includes
#
$(TARGDIR)/vers-includes: $(addsuffix .vers.h,$(EXT_TOOLS))

.PHONY: $(TARGDIR)/vers-includes

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# test-vdb-config
#

runtests: test-vdb-config model

test-vdb-config:
	@ printf "Testing exit code of $(DIRTOTEST)/vdb-config... "
	@ PATH=$(DIRTOTEST):"$(PATH)" && ./test-vdb-config.pl
	@ echo OK

#-------------------------------------------------------------------------------
# test-vdb-config-model
#

STRIDES_SRC = \
	test-vdb-config-model

STRIDES_OBJ = \
	$(addsuffix .$(OBJX),$(STRIDES_SRC))

STRIDES_LIB = \
	-skapp \
	-sktst \
	-sncbi-vdb \

$(BINDIR)/test-vdb-config-model: $(STRIDES_OBJ)
	$(LP) --exe -o $@ $^ $(STRIDES_LIB)

model: test-vdb-config-model
	NCBI_SETTINGS=`pwd`/tmp/u $(BINDIR)/test-vdb-config-model ; rm -r tmp


