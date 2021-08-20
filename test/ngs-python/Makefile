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

MODULE = test/ngs-python

ALL_TARGS = \

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# runtests
#
ifdef PYTHON
runtests: pytest

else
runtests: 

endif

pytest:
	$(PYTHON) -c "from ngs import NGS" \
		&& export VDB_CONFIG=$(VDB_CONFIG) ; export NGS_PY_DOWNLOAD_LIBRARY=0; \
			unset NGS_PY_LIBRARY_PATH; \
			export LD_LIBRARY_PATH=$(LIBDIR):$(VDB_LIBDIR):$$LD_LIBRARY_PATH; \
			NCBI_SETTINGS=/ $(PYTHON) tests.py \
		|| echo "NGS Python tests are not run since ngs module does not appear to be installed"

.PHONY: pytest
