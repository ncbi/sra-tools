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

std:

clean:

.PHONY: default std $(TARGETS)

PYTHON_VERS = $(shell python -V 2>&1)

# there is a separate set of sources for 2.5 and 2.6
ifeq (Python 2.6,$(shell expr "$(PYTHON_VERS)" : "\(Python 2.6\)"))
    PYTHON_VERS=26
else
    ifeq (Python 2.5,$(shell expr "$(PYTHON_VERS)" : "\(Python 2.5\)"))
        PYTHON_VERS=26
    else
        PYTHON_VERS=
    endif
endif

NGS_EXAMPLES = \
	FragTest \
	AlignTest \
	AlignSliceTest \
	PileupTest \
	RefTest \

# ===========================================================================
#
# example runs
#
# NOTE: Expect libngs-sdk.so and libncbi-vdb.so somewhere inside $LD_LIBRARY_PATH

run_frag:
	python FragTest$(PYTHON_VERS).py ERR225922 10000 2 $(REDIRECT)

run_align:
	python AlignTest$(PYTHON_VERS).py ERR225922 10000 2 $(REDIRECT)

run_align_slice:
	python AlignSliceTest$(PYTHON_VERS).py SRR1121656 1 1 9999 $(REDIRECT)

run_pileup:
	python PileupTest$(PYTHON_VERS).py SRR1121656 1 9999 10003 $(REDIRECT)

run_ref:
	python RefTest$(PYTHON_VERS).py SRR1121656 $(REDIRECT)

ALL_TESTS = run_frag run_align run_align_slice run_pileup run_ref

run_all: $(ALL_TESTS)

.PHONY: run_align run_align_slice run_frag

# ===========================================================================
#
# expected results for auto-testing
#
expected.txt update_expected:
	@if [ "$(PYTHON_VERS)" = "26" ] ; then \
		echo Python 2.6 is not supported ; \
	else \
		rm -f expected.txt; LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) $(MAKE) run_all REDIRECT=">>expected.txt" ; \
	fi

run_and_diff: expected.txt
	@if [ "$(PYTHON_VERS)" = "26" ] ; then \
		echo Python 2.6 is not supported ; \
	else \
		rm -f actual.txt; LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) $(MAKE) run_all REDIRECT=">>actual.txt" ; \
		diff expected.txt actual.txt && rm actual.txt && echo NGS Python examples work as expected ; \
	fi


.PHONY: update_expected run_and_diff
