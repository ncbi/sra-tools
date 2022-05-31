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

default: prepare_test_data  \
        check_test_result

TOP ?= $(abspath ../../..)

MODULE = test/general-loader/VDB-3250

TEST_TOOLS = \
	prepare_test_data   \
	check_test_result   \

ALL_TOOLS = \
	$(TEST_TOOLS) \

include $(TOP)/build/Makefile.env

INCDIRS += -I$(TOP)/tools/general-loader

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# chuck
#

PREPARE_DATA_SRC = \
	prepare_test_data

PREPARE_DATA_OBJ = \
	$(addsuffix .$(OBJX),$(PREPARE_DATA_SRC))

PREPARE_DATA_LIB =   \
	-sgeneral-writer  \
	-sncbi-vdb

$(TEST_BINDIR)/prepare_test_data: $(PREPARE_DATA_OBJ)
	$(LP) --exe -o $@ $^ $(PREPARE_DATA_LIB)


#-------------------------------------------------------------------------------
# chuck
#

CHECK_RESULT_SRC = \
	check_test_result

CHECK_RESULT_OBJ = \
	$(addsuffix .$(OBJX),$(CHECK_RESULT_SRC))

CHECK_RESULT_LIB =   \
	-sncbi-vdb

$(TEST_BINDIR)/check_test_result: $(CHECK_RESULT_OBJ)
	$(LP) --exe -o $@ $^ $(CHECK_RESULT_LIB)


