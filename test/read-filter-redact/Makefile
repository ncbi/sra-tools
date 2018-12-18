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
# ==============================================================================

default: runtests

TOP ?= $(abspath ../..)

include $(TOP)/build/Makefile.env

runtests: check_version

RUN=tmp-read-filter-redact-test-run
FLT=tmp-read-filter-redact-test-in

check_version: #/align-cache/CSRA_file
	# remove old test files
	@ if $(BINDIR)/vdb-unlock $(RUN) 2>/dev/null ; then echo ; fi
	@ rm -fr tmp-read-filter-redact-test-*


	# prepare sources
	@ echo 1 > $(FLT)
	@ $(BINDIR)/kar --extract ../align-cache/CSRA_file \
		--directory $(RUN)

	# make sure HISTORY meta does not exist
	@ if $(BINDIR)/kdbmeta tmp-read-filter-redact-test-run \
				-TSEQUENCE HISTORY 2>/dev/null; \
		then echo "error: HISTORY found in source metadata"; exit 1; fi

	# read-filter-redact
	@ $(BINDIR)/read-filter-redact -F$(FLT) $(RUN)

	# make sure HISTORY meta was created
	@ $(BINDIR)/kdbmeta tmp-read-filter-redact-test-run -TSEQUENCE HISTORY | \
		grep '^  <EVENT_1 build="' | grep '" run="' | \
		grep '" tool="read-filter-redact" vers="'

	# remove old test files
	@ $(BINDIR)/vdb-unlock $(RUN)
	@ rm -fr tmp-read-filter-redact-test-*
