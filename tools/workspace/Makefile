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
MODULE = tools/workspace

include $(TOP)/build/Makefile.env

INT_TOOLS = \
	path-to-kar-file

EXT_TOOLS = \
	sra

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
all std: vers-includes
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(ALL_TOOLS): vers-includes
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(EXT_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# all
#
$(TARGDIR)/all: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/all

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
# sratoolkit
#
.PRECIOUS = \
	prefs-yacc.c \
	prefs-lex.c

TOOLKIT_SRC = \
	prefs-yacc \
	prefs-lex \
	sra \
	token \
	sratoolkit-exception \


TOOLKIT_OBJ = \
	$(addsuffix .$(OBJX),$(TOOLKIT_SRC))

TOOLKIT_LIB = \
	-skapp \
	-sncbi-vdb

$(BINDIR)/sra: $(TOOLKIT_OBJ)
	$(LP) --exe --vers $(SRCDIR) -o $@ $^ $(TOOLKIT_LIB)

#-------------------------------------------------------------------------------
# testing utilities
#
PATH2KAR_SRC = \
	path-to-kar-file \
	sratoolkit-exception


PATH2KAR_OBJ = \
	$(addsuffix .$(OBJX),$(PATH2KAR_SRC))

PATH2KAR_LIB = \
	-skapp \
	-sncbi-vdb

$(BINDIR)/path-to-kar-file: $(PATH2KAR_OBJ)
	$(LP) --exe --vers $(SRCDIR) -o $@ $^ $(PATH2KAR_LIB)
