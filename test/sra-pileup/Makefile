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

MODULE = test/sra-pileup

TEST_TOOLS = 

include $(TOP)/build/Makefile.env

CONFIGTOUSE ?= NCBI_SETTINGS
DIRTOTEST ?= $(BINDIR)

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

ifdef PYTHON
runtests: announce check_exit_code check_skiplist

slowtests: announce fastq_dump_vs_sam_dump sam_dump_spotgroup_for_all

else
runtests: ;

slowtests: ;

endif

announce:
	@echo Testing $(DIRTOTEST)

#-------------------------------------------------------------------------------
# scripted tests
#

check_exit_code:
	@ $(CONFIGTOUSE)=/ $(PYTHON) \
	    $(TOP)/build/check-exit-code.py $(DIRTOTEST)/sra-pileup

check_skiplist:
	@ $(CONFIGTOUSE)=/ $(PYTHON) check_skiplist.py $(DIRTOTEST)/sra-pileup

ACC = SRR3332402

#-------------------------------------------------------------------------------
# testing if sam-dump and fastq-dump produce the same READ/QUALITY values
#
fastq_dump_vs_sam_dump :
	@ $(CONFIGTOUSE)=/ $(PYTHON) test_diff_fastq_dump_vs_sam_dump.py \
	    -a $(ACC) -f $(DIRTOTEST)/fastq-dump -m $(DIRTOTEST)/sam-dump

#-------------------------------------------------------------------------------
# testing if sam-dump and fastq-dump produce the same READ/QUALITY values
#
sam_dump_spotgroup_for_all :
	@ $(CONFIGTOUSE)=/ $(PYTHON) test_all_sam_dump_has_spotgroup.py \
	  -a $(ACC) -m $(DIRTOTEST)/sam-dump

.PHONY: $(TEST_TOOLS)

clean: stdclean
