:: ===========================================================================
::
::                            PUBLIC DOMAIN NOTICE
::               National Center for Biotechnology Information
::
::  This software/database is a "United States Government Work" under the
::  terms of the United States Copyright Act.  It was written as part of
::  the author's official duties as a United States Government employee and
::  thus cannot be copyrighted.  This software/database is freely available
::  to the public for use. The National Library of Medicine and the U.S.
::  Government have not placed any restriction on its use or reproduction.
::
::  Although all reasonable efforts have been taken to ensure the accuracy
::  and reliability of the software and data, the NLM and the U.S.
::  Government do not and cannot warrant the performance or results that
::  may be obtained by using this software or data. The NLM and the U.S.
::  Government disclaim all warranties, express or implied, including
::  warranties of performance, merchantability or fitness for any particular
::  purpose.
::
::  Please cite the author in any work or product based on this material.
::
:: ===========================================================================

echo off
Setlocal
Setlocal EnableDelayedExpansion

set TOOLS=abi-dump align-info blastn_vdb cache-mgr fastq-dump illumina-dump kar kdbmeta latf-load prefetch rcexplain sam-dump sff-dump ^
          sra-kar sra-pileup sra-stat srapath tblastn_vdb test-sra vdb-config vdb-copy vdb-decrypt vdb-dump vdb-encrypt vdb-lock ^
          vdb-unlock vdb-validate   

:: vdb-passwd is obsolete but still in the package

powershell -Command wget http://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/sratoolkit.current-win64.zip -Outfile sratoolkit.current-win64.zip || exit /b 1
jar xf sratoolkit.current-win64.zip || exit /b 2

cd sratoolkit.2.5.2-win64\bin

set FAILED=

for %%t in ( %TOOLS% ) do ( echo | set /p=%%t & %%t -h >NUL & ( if errorlevel 1 ( echo | set /p=failed! & set FAILED=!FAILED! %%t ) ) & echo. )

if "%FAILED%" NEQ "" ( echo. & echo FAILED: %FAILED% & exit /b 3 )

cd ..\..
rmdir /S /Q sratoolkit.2.5.2-win64
del /Q sratoolkit.current-win64.zip

exit /b 0
