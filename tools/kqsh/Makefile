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
MODULE = tools/kqsh

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	kqsh

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs kqsh-thunks
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

$(ALL_TOOLS): makedirs kqsh-thunks
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean
	@ rm -f $(SRCDIR)/kqsh-[kv]db.[ch]

.PHONY: clean

#-------------------------------------------------------------------------------
# kqsh-thunks
#
KDB_THUNK_SRC = \
	manager.h \
	database.h \
	table.h \
	column.h \
	index.h \
	meta.h \
	namelist.h

KDB_THUNK_INCS = \
	$(addprefix kdb/,$(KDB_THUNK_SRC))

KDB_THUNK_DEPS = \
	$(addprefix $(ITF)/,$(KDB_THUNK_INCS)) \
	kqsh-thunk.pl \
	Makefile

KDB_THUNK_OBJ = \
	:KDBManager=mgr \
	:KDatabase=db \
	:KTable=tbl \
	:KColumn=col \
	:KColumnBlob=blob \
	:KIndex=idx \
	:KMetadata=meta \
	:KMDataNode=node

kqsh-kdb.c: $(KDB_THUNK_DEPS)
	perl kqsh-thunk.pl -mkdb -I$(ITF) $(addprefix -i,$(KDB_THUNK_INCS)) \
	  $(KDB_THUNK_OBJ) -h kqsh-kdb.h -c $@ || rm $@

kqsh-kdb.h: kqsh-kdb.c

VDB_THUNK_SRC = \
	manager.h \
	database.h \
	schema.h \
	table.h \
	cursor.h

VDB_THUNK_INCS = \
	$(addprefix vdb/,$(VDB_THUNK_SRC))

VDB_THUNK_DEPS = \
	$(addprefix $(ITF)/,$(VDB_THUNK_INCS)) \
	kqsh-thunk.pl \
	Makefile

VDB_THUNK_OBJ = \
	:VDBManager=mgr \
	VSchemaRuntimeTable=srtt \
	VSchema=schema \
	VTypedecl=td \
	VTypedesc=tdesc \
	:VDatabase=db \
	:VTable=tbl \
	:VCursor=curs

kqsh-vdb.c: $(VDB_THUNK_DEPS)
	perl kqsh-thunk.pl -mvdb -I$(ITF) $(addprefix -i,$(VDB_THUNK_INCS)) \
	  $(VDB_THUNK_OBJ) -h kqsh-vdb.h -c $@ || rm $@

kqsh-vdb.h: kqsh-vdb.c

SRA_THUNK_SRC = \
	sradb.h \
	wsradb.h \
	sraschema.h

SRA_THUNK_INCS = \
	$(addprefix sra/,$(SRA_THUNK_SRC))

SRA_THUNK_DEPS = \
	$(addprefix $(ITF)/,$(SRA_THUNK_INCS)) \
	kqsh-thunk.pl \
	Makefile

SRA_THUNK_OBJ = \
	:SRAMgr=mgr \
	SRANamelist=nmlist \
	:SRATable=tbl \
	SRAColumn=col \
	VDBManager=vmgr

kqsh-sra.c: $(SRA_THUNK_DEPS)
	perl kqsh-thunk.pl -msra -I$(ITF) $(addprefix -i,$(SRA_THUNK_INCS)) \
	  $(SRA_THUNK_OBJ) -h kqsh-sra.h -c $@ || rm $@

kqsh-sra.h: kqsh-sra.c

kqsh-thunks: kqsh-kdb.h kqsh-vdb.h kqsh-sra.h

#-------------------------------------------------------------------------------
# kqsh-load
#  special rules to tell kqsh how to load libraries
#

#KQSH_LIBNAMES = \
	-DLIBKDB=$(LPFX)kdb.$(SHLX) \
	-DLIBWKDB=$(LPFX)wkdb.$(SHLX) \
	-DLIBVDB=$(LPFX)vdb.$(SHLX) \
	-DLIBWVDB=$(LPFX)wvdb.$(SHLX) \
	-DLIBSRADB=$(LPFX)sradb.$(SHLX) \
	-DLIBWSRADB=$(LPFX)wsradb.$(SHLX)

KQSH_LIBNAMES = \
	-DLIBNCBI_VDB=$(LPFX)ncbi-vdb.$(SHLX) \
	-DLIBNCBI_WVDB=$(LPFX)ncbi-wvdb.$(SHLX)

kqsh-load.$(OBJX): $(SRCDIR)/kqsh-load.c
	$(CC) -o $@ $(OPT) $(KQSH_LIBNAMES) -D_LOGGING $<


#-------------------------------------------------------------------------------
# kqsh - query shell
#
KQSH_SRC = \
	kqsh-print \
	kqsh-tok \
	kqsh-parse \
	kqsh-alter \
	kqsh-close \
	kqsh-help \
	kqsh-load \
	kqsh-open \
	kqsh-create \
	kqsh-show \
	kqsh-write \
	kqsh-kdb \
	kqsh-vdb \
	kqsh-sra \
	kqsh

KQSH_OBJ = \
	$(addsuffix .$(OBJX),$(KQSH_SRC))

KQSH_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \


$(BINDIR)/kqsh: $(KQSH_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(KQSH_LIB)
