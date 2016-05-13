# ==============================================================================
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

USE_GCC_IF_ICC = 1

TOP ?= $(abspath ../..)
MODULE = tools/vdb-decrypt

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	vdb-decrypt \
	vdb-encrypt

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)




#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# vdb-decrypt
#  Copy files from one directory to another while cataloging the contents.
#  XML files can be redirected as well.
#
VDB_DECRYPT_SRC = \
	vdb-decrypt \
	shared

VDB_DECRYPT_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_DECRYPT_SRC))

VDB_DECRYPT_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb

$(BINDIR)/vdb-decrypt: $(VDB_DECRYPT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_DECRYPT_LIB)

#-------------------------------------------------------------------------------
# vdb-encrypt
#  Copy files from one directory to another while cataloging the contents.
#  XML files can be redirected as well.
#
VDB_ENCRYPT_SRC = \
	vdb-encrypt \
	shared

VDB_ENCRYPT_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_ENCRYPT_SRC))

VDB_ENCRYPT_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb

$(BINDIR)/vdb-encrypt: $(VDB_ENCRYPT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_ENCRYPT_LIB)

