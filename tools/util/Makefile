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
MODULE = tools/util

INT_TOOLS = \
	testld \
	txt2kdb \
	rowwritetest \
	sortreadtest \
	pacbio-correct \
	crc32sum \
	samview \
	kdb-index \
	pseudo-aligner \
	schema-replace \

EXT_TOOLS = \
	rcexplain \
	kdbmeta \
	md5cp \
	vdb-lock \
	vdb-unlock \
	test-sra \
	vdb-passwd \

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)
	
include $(TOP)/build/Makefile.env

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
# rcexplain - explain a return code observed in debugger
#
RCEXPLAIN_SRC = \
	rcexplain

RCEXPLAIN_OBJ = \
	$(addsuffix .$(OBJX),$(RCEXPLAIN_SRC))

RCEXPLAIN_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \

$(BINDIR)/rcexplain: $(RCEXPLAIN_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(RCEXPLAIN_LIB)


#-------------------------------------------------------------------------------
# kdbmeta - query and update metadata files
#
KDBMETA_SRC = \
	kdbmeta

KDBMETA_OBJ = \
	$(addsuffix .$(OBJX),$(KDBMETA_SRC))

KDBMETA_LIB = \
	-skapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/kdbmeta: $(KDBMETA_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(KDBMETA_LIB)


#-------------------------------------------------------------------------------
# vdb-lock
#  lock a vdb object
#
VDB_LOCK_SRC = \
	vdb-lock

VDB_LOCK_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_LOCK_SRC))

VDB_LOCK_LIB = \
	-skapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/vdb-lock: $(VDB_LOCK_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_LOCK_LIB)


#-------------------------------------------------------------------------------
# vdb-unlock
#  unlock a vdb object
#
VDB_UNLOCK_SRC = \
	vdb-unlock

VDB_UNLOCK_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_UNLOCK_SRC))

VDB_UNLOCK_LIB = \
	-skapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/vdb-unlock: $(VDB_UNLOCK_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_UNLOCK_LIB)


#-------------------------------------------------------------------------------
# testld - test dynamic load of libraries
#
TESTLD_SRC = \
	testld

TESTLD_OBJ = \
	$(addsuffix .$(OBJX),$(TESTLD_SRC))

TESTLD_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \


$(BINDIR)/testld: $(TESTLD_OBJ)
	$(LD) --exe -o $@ $^ $(TESTLD_LIB)


#-------------------------------------------------------------------------------
# test-sra: print config, resolve sra, check dependencies
#

T_SRC = \
	PrintOS \
	test-sra \

T_OBJ = \
	$(addsuffix .$(OBJX),$(T_SRC))

T_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb \
	-lm

$(BINDIR)/test-sra: $(T_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(T_LIB)

#-------------------------------------------------------------------------------
# txt2kdb - test dynamic load of libraries
#
TXT2KDB_SRC = \
	txt2kdb

TXT2KDB_OBJ = \
	$(addsuffix .$(OBJX),$(TXT2KDB_SRC))

TXT2KDB_LIB = \
	-skapp \
	-stk-version \
	-sncbi-wvdb \


$(BINDIR)/txt2kdb: $(TXT2KDB_OBJ)
	$(LD) --exe -o $@ $^ $(TXT2KDB_LIB)

#----------------------------------------------------------------
# md5cp
#
MD5CP_SRC = \
	md5cp

MD5CP_OBJ = \
	$(addsuffix .$(OBJX),$(MD5CP_SRC))

MD5CP_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \


$(BINDIR)/md5cp: $(MD5CP_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(MD5CP_LIB)


#----------------------------------------------------------------
# rowwritetest
#
ROWWRITETEST_SRC = \
	rowwritetest

ROWWRITETEST_OBJ = \
	$(addsuffix .$(OBJX),$(ROWWRITETEST_SRC))

ROWWRITETEST_LIB = \
	-skapp \
	-stk-version \
	-sncbi-wvdb \


$(BINDIR)/rowwritetest: $(ROWWRITETEST_OBJ)
	$(LD) --exe -o $@ $^ $(ROWWRITETEST_LIB)


#-------------------------------------------------------------------------------
# crc32sum 
#  behaves a lot like md5sum
#
CRC32SUM_SRC = \
	crc32sum

CRC32SUM_OBJ = \
	$(addsuffix .$(OBJX),$(CRC32SUM_SRC))

CRC32SUM_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb \


$(BINDIR)/crc32sum: $(CRC32SUM_OBJ)
	$(LD) --exe -o $@ $^ $(CRC32SUM_LIB)


#-------------------------------------------------------------------------------
# samview
#  for testing bam/sam code
#
SAMVIEW_SRC = \
	samview

SAMVIEW_OBJ = \
	$(addsuffix .$(OBJX),$(SAMVIEW_SRC))

SAMVIEW_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-vdb \


$(BINDIR)/samview: $(SAMVIEW_OBJ)
	$(LD) --exe -o $@ $^ $(SAMVIEW_LIB)


#-------------------------------------------------------------------------------
# pseudo-aligner
#  for generating test data
#
SUDO_ALGN_SRC = \
	pseudo-aligner

SUDO_ALGN_OBJ = \
	$(addsuffix .$(OBJX),$(SUDO_ALGN_SRC))

$(BINDIR)/pseudo-aligner: $(SUDO_ALGN_OBJ)
	$(LP) --exe -o $@ $^


#----------------------------------------------------------------
# sortreadtest
#
SORTREADTEST_SRC = \
	sortreadtest

SORTREADTEST_OBJ = \
	$(addsuffix .$(OBJX),$(SORTREADTEST_SRC))

SORTREADTEST_LIB = \
	-skapp \
	-stk-version \
	-sncbi-wvdb \


$(BINDIR)/sortreadtest: $(SORTREADTEST_OBJ)
	$(LD) --exe -o $@ $^ $(SORTREADTEST_LIB)


#----------------------------------------------------------------
# vdb-passwd
#
VDB_PASSWD_SRC = \
	vdb-passwd \
	syspass

VDB_PASSWD_OBJ = \
	$(addsuffix .$(OBJX),$(VDB_PASSWD_SRC))

VDB_PASSWD_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \


$(BINDIR)/vdb-passwd: $(VDB_PASSWD_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VDB_PASSWD_LIB)


#----------------------------------------------------------------
# schema-replace
#
SCHEMA_REPLACE_SRC = \
	schema-replace

SCHEMA_REPLACE_OBJ = \
	$(addsuffix .$(OBJX),$(SCHEMA_REPLACE_SRC))

SCHEMA_REPLACE_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/schema-replace: $(SCHEMA_REPLACE_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(SCHEMA_REPLACE_LIB)

#-------------------------------------------------------------------------------
# pacbio-correct-tool
#
PACBIO_CORRECT_SRC = \
	pacbio-correct

PACBIO_CORRECT_OBJ = \
	$(addsuffix .$(OBJX),$(PACBIO_CORRECT_SRC))

PACBIO_CORRECT_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-wvdb \


$(BINDIR)/pacbio-correct: $(PACBIO_CORRECT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(PACBIO_CORRECT_LIB)

#-------------------------------------------------------------------------------
# kdb-index
#
KDBINDEX_SRC = \
	kdb-index

KDBINDEX_OBJ = \
	$(addsuffix .$(OBJX),$(KDBINDEX_SRC))

KDBINDEX_LIB = \
	-skapp \
	-stk-version \
	-sncbi-vdb \

$(BINDIR)/kdb-index: $(KDBINDEX_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(KDBINDEX_LIB)
