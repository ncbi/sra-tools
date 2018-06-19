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
MODULE = tools/sra-toolkit

INT_TOOLS = \

EXT_TOOLS = \
	sra-toolkit

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

ifeq (,$(QMAKE_BIN))
all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built:"               \
	       "It requires Qt to be installed"
else

#-------------------------------------------------------------------------------
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
# sra-toolkit
#

ifeq (dbg,$(BUILD))
   QT_CONF = debug
else
   QT_CONF = release
endif


$(BINDIR)/sra-toolkit:
	$(QMAKE_BIN) $(SRCDIR)/sra-toolkit.pro CONFIG+=$(QT_CONF) CONFIG+=make
	make -C $(OBJDIR)
	cp $(OBJDIR)/sra-toolkit $(BINDIR)/sra-toolkit$(VERSION_EXEX)
	ln -s sra-toolkit$(VERSION_EXEX) $(BINDIR)/sra-toolkit$(MAJVERS_EXEX)
	ln -s sra-toolkit$(MAJVERS_EXEX) $(BINDIR)/sra-toolkit
endif
