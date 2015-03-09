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

ALL_TOOLS = \
	general-loader \

#-------------------------------------------------------------------------------
# outer targets
#
all std: vers-includes
	@ $(MAKE_CMD) $(TARGDIR)/std

$(ALL_TOOLS): vers-includes
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(LIBDIR)/,$(ALL_LIBS)) \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

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
# general-writer
#
$(LIBDIR)/general-writer: $(LIBDIR)/$(LPFX)general-writer.$(LIBX)

GEN_WRITER_SRC = \
    general-writer

GEN_WRITER_OBJ = \
	$(addsuffix .$(LOBX),$(GEN_WRITER_SRC))

$(LIBDIR)/$(LPFX)general-writer.$(LIBX): $(GEN_WRITER_OBJ)
	$(LD) --slib --vers $(SRCDIR) -o $@ $^

#-------------------------------------------------------------------------------
# general-loader
#
GEN_LOAD_SRC = \
    general-loader \
	main \

GEN_LOAD_OBJ = \
	$(addsuffix .$(OBJX),$(GEN_LOAD_SRC))

GEN_LOAD_LIBS = \
	-sncbi-wvdb-static \
	-skapp \

$(BINDIR)/general-loader: $(GEN_LOAD_OBJ)
	$(LP) --exe --vers $(SRCDIR) -o $@ $^ $(GEN_LOAD_LIBS)

