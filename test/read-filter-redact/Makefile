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

DIRTOTEST ?= $(BINDIR)
TOOLTOTEST = $(DIRTOTEST)/read-filter-redact

ifneq ("$(wildcard $(TOOLTOTEST))","")
    BIN_EXISTS = 1
else
    BIN_EXISTS = 0
endif

ifeq (0,$(BIN_EXISTS))
runtests: no-test
else
runtests: announce check_version
endif

RUN=tmp-read-filter-redact-test-run
FLT=tmp-read-filter-redact-test-in

no-test:
	@ echo $(TOOLTOTEST) does not exist. Test skipped.

announce:
	@ echo Testing $(DIRTOTEST)

check_version: #/align-cache/CSRA_file
	# remove old test files
	@ if $(DIRTOTEST)/vdb-unlock $(RUN) 2>/dev/null ; then echo ; fi
	@ rm -fr tmp-read-filter-redact-test-*


	# prepare sources
	@ echo 1 > $(FLT)
	@ $(DIRTOTEST)/kar --extract ../align-cache/CSRA_file \
		--directory $(RUN)

	# make sure HISTORY meta does not exist
	@ if $(DIRTOTEST)/kdbmeta tmp-read-filter-redact-test-run \
				-TSEQUENCE HISTORY 2>/dev/null; \
		then echo "error: HISTORY found in source metadata"; exit 1; fi

	# read-filter-redact
	@ $(DIRTOTEST)/read-filter-redact -F$(FLT) $(RUN) > /dev/null 2>&1

	# make sure HISTORY meta was created
	@ $(DIRTOTEST)/kdbmeta tmp-read-filter-redact-test-run -TSEQUENCE HISTORY | \
		grep '^  <EVENT_1 build="' | grep '" run="' | \
		grep '" tool="read-filter-redact" vers="' > /dev/null

	# remove old test files
	@ $(DIRTOTEST)/vdb-unlock $(RUN)
	@ rm -fr tmp-read-filter-redact-test-*
