@rem # ===========================================================================
@rem #
@rem #                            PUBLIC DOMAIN NOTICE
@rem #               National Center for Biotechnology Information
@rem #
@rem #  This software/database is a "United States Government Work" under the
@rem #  terms of the United States Copyright Act.  It was written as part of
@rem #  the author's official duties as a United States Government employee and
@rem #  thus cannot be copyrighted.  This software/database is freely available
@rem #  to the public for use. The National Library of Medicine and the U.S.
@rem #  Government have not placed any restriction on its use or reproduction.
@rem #
@rem #  Although all reasonable efforts have been taken to ensure the accuracy
@rem #  and reliability of the software and data, the NLM and the U.S.
@rem #  Government do not and cannot warrant the performance or results that
@rem #  may be obtained by using this software or data. The NLM and the U.S.
@rem #  Government disclaim all warranties, express or implied, including
@rem #  warranties of performance, merchantability or fitness for any particular
@rem #  purpose.
@rem #
@rem #  Please cite the author in any work or product based on this material.
@rem #
@rem # ===========================================================================

@set BINDIR=%1%
@set DRIVE=%~d1%
@set CURDIR=%CD%
@set TEMPDIR=%CURDIR%\temp
@set ACTUAL=%CURDIR%\actual
@set EXPECTED=%CURDIR%\expected

@md %ACTUAL% 2>NUL
@md %TEMPDIR% 2>NUL
@echo '/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"' >%TEMPDIR%/tmp.mkfg
@set NCBI_SETTING=%TEMPDIR%/tmp.mkfg

@rem # ===========================================================================
@echo running built-in tests

@set SRATOOLS_TESTING=1
@%BINDIR%\sratools
@if ERRORLEVEL   1 goto FAILED
@set SRATOOLS_TESTING=


@rem # ===========================================================================
@echo testing expected output for unknown tool

@set SRATOOLS_IMPERSONATE=rcexplain
@%BINDIR%\sratools 2>%ACTUAL%/bogus.stderr
@fc /L %EXPECTED%\bogus.stderr %ACTUAL%\bogus.stderr > NUL 2> NUL
@if ERRORLEVEL   1 goto FAILED
@set SRATOOLS_IMPERSONATE=


@rem # ===========================================================================
@set SRATOOLS_IMPERSONATE=fastq-dump
@for %%C in (SRP000001 SRX000001 SRS000001 SRA000001 ERP000001 DRX000001 ) do @(
    echo testing expected output for container %%C
    %BINDIR%\sratools %%C 2>%ACTUAL%\%%C.stderr
    fc /L %EXPECTED%\%%C.stderr %ACTUAL%\%%C.stderr > NUL 2> NUL
    if ERRORLEVEL   1 goto FAILED
)
@set SRATOOLS_IMPERSONATE=


@rem # ===========================================================================
@rem SRATOOLS_TESTING=2 skip SDL, sub-tool invocation is simulated to always
@rem succeed, but everything up to the exec call is real
@set SRATOOLS_TESTING=2
@for %%C in ( fastq-dump fasterq-dump sam-dump sra-pileup vdb-dump prefetch srapath ) do @(
    echo testing expected output for dry run of %%C
    set SRATOOLS_IMPERSONATE=%%C
	%BINDIR%/sratools -v SRR000001 ERR000001 DRR000001 2>%ACTUAL%\%%C.stderr
    if ERRORLEVEL   1 goto FAILED
    @set SRATOOLS_IMPERSONATE=
    fc /L %EXPECTED%\%%C.stderr %ACTUAL%\%%C.stderr > NUL 2> NUL
    if ERRORLEVEL   1 goto FAILED
)
@set SRATOOLS_TESTING=

@rem # ===========================================================================
@rem path to the executable with a drive letter
@cd %BINDIR%
@echo testing expected output for %DRIVE%fastq-dump
@%DRIVE%fastq-dump -Z SRR000123 >NUL 2>%ACTUAL%\drive.stderr
@fc /L %EXPECTED%\drive.stderr%ACTUAL%\drive.stderr > NUL 2> NUL
@if ERRORLEVEL   1 goto FAILED
@cd %CURDIR%

@rem # ===========================================================================
@rem all tests passed
@cd %CURDIR%
@rd /Q /S %TEMPDIR%
@exit /B 0

@rem # ===========================================================================
:FAILED
@echo some tests failed!

cd %CURDIR%
@exit /B 1