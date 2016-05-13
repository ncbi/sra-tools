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
MODULE = tools/vdb-copy

include $(TOP)/build/Makefile.env

INT_TOOLS = \

EXT_TOOLS = \
	vdb-copy

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

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

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# vdb-copy
#  vdb copy tool
#
$(BINDIR)/vdb-copy: $(NCBIDIR)/vdb-copy.kfg 

$(NCBIDIR)/vdb-copy.kfg: $(SRCDIR)/vdb-copy.kfg
	cp $^ $@

VDB_COPY_SRC = \
	context \
	helper \
	coldefs \
	get_platform \
	namelist_tools \
	copy_meta \
	type_matcher \
	redactval \
	config_values \
	vdb-copy

VDB_COPY_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_COPY_SRC))

VDB_COPY_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/vdb-copy: $(VDB_COPY_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_COPY_LIB)
