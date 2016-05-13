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

MODULE = tools/general-loader

include $(TOP)/build/Makefile.env

ALL_LIBS = \
	general-writer

INT_TOOLS = \
	gw-dumper

EXT_TOOLS = \
	general-loader

ALL_TOOLS =      \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(INT_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

$(EXT_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(LIBDIR)/,$(ALL_LIBS)) \
	$(addprefix $(TEST_BINDIR)/,$(INT_TOOLS)) \
	$(addprefix $(BINDIR)/,$(EXT_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# general-writer
#
$(LIBDIR)/general-writer: $(LIBDIR)/$(LPFX)general-writer.$(LIBX)

GEN_WRITER_SRC = \
    general-writer \
	utf8-like-int-codec

GEN_WRITER_OBJ = \
	$(addsuffix .$(LOBX),$(GEN_WRITER_SRC))

$(LIBDIR)/$(LPFX)general-writer.$(LIBX): $(GEN_WRITER_OBJ)
	$(LD) --slib --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^

#-------------------------------------------------------------------------------
# gw-dumper
#

$(TEST_BINDIR)/gw-dumper: $(SRCDIR)/gw-dumper.cpp $(SRCDIR)/utf8-like-int-codec.c
	c++ $(DBG) -o $@ $^ -I$(SRCDIR) -I$(VDB_INCDIR)/os/$(OS) -I$(VDB_INCDIR)


#-------------------------------------------------------------------------------
# general-loader
#
GEN_LOAD_SRC = \
    general-loader \
    protocol-parser \
    database-loader \
	utf8-like-int-codec \
	main \

GEN_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(GEN_LOAD_SRC))

GEN_LOAD_LIBS = \
	-sncbi-wvdb-static \
	-sload \
	-skapp \
	-stk-version \

$(BINDIR)/general-loader: $(GEN_LOAD_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(GEN_LOAD_LIBS)

