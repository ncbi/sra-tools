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
	@ rm tmp.kfg

TOP ?= $(abspath ../..)

MODULE = test/vschema

include $(TOP)/build/Makefile.env

$(shell echo "vdb/schema/paths = \"$(VDB_INCDIR)\"" > tmp.kfg)

clean: stdclean

#-------------------------------------------------------------------------------
# bio-end schema tests
#
runtests: \
	bio-end-tests \
    idx-text-tests

#-------------------------------------------------------------------------------
# bio-end-tests
#
TEST_BIO_END_MAKEINPUTS_SRC = \
	bio-end/makeinputs

TEST_BIO_END_MAKEINPUTS_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_BIO_END_MAKEINPUTS_SRC))

TEST_BIO_END_MAKEINPUTS_LIB =   \
	-L$(LIBDIR) \
	-sgeneral-writer


bio-end-makeinputs: $(TEST_BIO_END_MAKEINPUTS_OBJ)
	$(LP) --exe -o ./bio-end-makeinputs $^ $(TEST_BIO_END_MAKEINPUTS_LIB)
	@rm bio-end-makeinputs.exe.d $(TEST_BIO_END_MAKEINPUTS_OBJ)

bio-end-tests: bio-end-makeinputs
	@mkdir -p $(SRCDIR)/bio-end/input && ./bio-end-makeinputs && rm ./bio-end-makeinputs
	@# test cases
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/bio-end bio-end excl-1 0 n
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/bio-end bio-end excl-2 0 n
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/bio-end bio-end excl-3 0 n
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/bio-end bio-end excl-4 0 n
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/bio-end bio-end incl-1 0 n
	@rm -f $(SRCDIR)/bio-end/input/*.gl
	@echo "bio-end tests executed successfuly"

#-------------------------------------------------------------------------------
# idx-text-tests
#
TEST_IDX_TEXT_MAKEINPUTS_SRC = \
	idx-text/makeinputs

TEST_IDX_TEXT_MAKEINPUTS_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_IDX_TEXT_MAKEINPUTS_SRC))

TEST_IDX_TEXT_MAKEINPUTS_LIB =   \
	-L$(LIBDIR) \
	-sgeneral-writer

idx-text-makeinputs: $(TEST_IDX_TEXT_MAKEINPUTS_OBJ)
	$(LP) --exe -o ./idx-text-makeinputs $^ $(TEST_IDX_TEXT_MAKEINPUTS_LIB)
	@rm idx-text-makeinputs.exe.d $(TEST_IDX_TEXT_MAKEINPUTS_OBJ)


TEST_IDX_TEXT_CHECKLOOKUP_SRC = \
	idx-text/checklookup

TEST_IDX_TEXT_CHECKLOOKUP_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_IDX_TEXT_CHECKLOOKUP_SRC))

TEST_IDX_TEXT_CHECKLOOKUP_LIB =   \
	-L$(LIBDIR) \
	-sncbi-vdb

idx-text-checklookup: $(TEST_IDX_TEXT_CHECKLOOKUP_OBJ)
	$(LP) --exe -o ./idx-text-checklookup $^ $(TEST_IDX_TEXT_CHECKLOOKUP_LIB)
	@rm idx-text-checklookup.exe.d $(TEST_IDX_TEXT_CHECKLOOKUP_OBJ)


idx-text-tests: idx-text-makeinputs idx-text-checklookup
	@mkdir -p $(SRCDIR)/idx-text/input && ./idx-text-makeinputs && rm ./idx-text-makeinputs
	@# test cases
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text ci-1 0 y ./idx-text-checklookup
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text ci-2 0 y ./idx-text-checklookup
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text ci-3 0 y ./idx-text-checklookup
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text ci-4 0 y ./idx-text-checklookup
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text ci-5 0 n
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text cs-1 0 y ./idx-text-checklookup
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text cs-2 0 y ./idx-text-checklookup
	@$(SRCDIR)/runtestcase.sh $(BINDIR) $(SRCDIR)/idx-text idx-text cs-3 0 n
	@rm -f ./idx-text-checklookup
	@rm -f $(SRCDIR)/idx-text/input/*.gl
	@echo "idx-text tests executed successfuly"
