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

TOP ?= $(abspath ../..)

MODULE = test/vdb-validate

TEST_TOOLS = \

ALL_TOOLS = \
	$(TEST_TOOLS) \

include $(TOP)/build/Makefile.env

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# ref-variation tool tests
#
runtests: vdb-validate

vdb-validate: $(BINDIR)/vdb-validate
	@ echo "Starting vdb-validate tests..."
	@ rm -rf actual/
	@ mkdir actual/

	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_len_mismatch.csra" no_sdc_checks 0
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_tmp_mismatch.csra --sdc:rows 100%" sdc_tmp_mismatch 3
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_pa_longer.csra --sdc:rows 100%" sdc_pa_longer_1 3
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_pa_longer.csra --sdc:rows 100% --sdc:plen_thold 50%" sdc_pa_longer_2 3
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_pa_longer.csra --sdc:rows 100% --sdc:plen_thold 51%" sdc_pa_longer_3 0
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_len_mismatch.csra --sdc:rows 100% --sdc:plen_thold 1%" sdc_len_mismatch_1 3
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_len_mismatch.csra --sdc:rows 100% --sdc:plen_thold 100%" sdc_len_mismatch_2 3
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_seq_cmp_read_len_corrupt.csra --sdc:seq-rows 100%" sdc_seq_cmp_read_len_corrupt 3
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/sdc_seq_cmp_read_len_fixed.csra --sdc:seq-rows 100%" sdc_seq_cmp_read_len_fixed 0
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/blob-row-gap.kar" ROW_GAP 0
	@ ./runtestcase.sh "$(BINDIR)/vdb-validate db/SRR053325 -Cyes" CONSISTENCY 0

	@ if [ "$(TEST_DATA)" != "" ]; then ./runtestcase.sh \
	    "$(BINDIR)/vdb-validate \
	                $(TEST_DATA)/SRR1207586-READ_LEN-vs-READ-mismatch \
	                -Cyes" READ_LEN 3 ; fi

	@ echo "All vdb-validate tests succeed"
	@ rm -rf actual/

