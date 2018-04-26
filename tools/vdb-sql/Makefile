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
MODULE = tools/vdb-sql

INT_TOOLS = \

EXT_TOOLS = \
	vdb-sql \
	fastconv

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (1,$(HAVE_XML2))

all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

else
all std: $(ALL_TOOLS)

$(ALL_TOOLS):
	@ echo "NOTE - $(@F) cannot be built: it requires 'libxml2' library."
endif

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# the tool: vdb-sql
# ( a version of the sqlite3-commandline-tool with the vdb-lib statically linked into it )
#
#   vdb_shellc. .... the originial sqlite3.c preloaded with the vdb-virtual table
#                    and enhanced by additional commandline-arguments

VDB_SQL_SRC = \
	vdb_shell

VDB_SQL_LIB = \
	-svdb-sqlite \
	-lm \
	-lpthread

VDB_SQL_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_SQL_SRC))

$(BINDIR)/vdb-sql: $(VDB_SQL_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_SQL_LIB)

#-------------------------------------------------------------------------------
# the tool: fastconv
FASTCONV_SRC = \
	fastconv

FASTCONV_LIB = \
	-skapp \
	-stk-version \
	-svdb-sqlite \
	-lm \
	-lpthread

FASTCONV_OBJ = \
	$(addsuffix .$(OBJX),$(FASTCONV_SRC))

$(BINDIR)/fastconv: $(FASTCONV_OBJ)
	$(LD) --exe -o $@ $^ $(FASTCONV_LIB)
