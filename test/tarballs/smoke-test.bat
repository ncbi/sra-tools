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

@echo off

set FAILED=

echo Smoke testing ngs tarball in %3 ...

if exist %3\ngs-java\ngs-java.jar (
    echo ngs-java.jar exists
) else (
    set FAILED=%FAILED% ngs-java.jar doesn't exist;
)

if exist %3\ngs-java\ngs-doc.jar (
    rem ngs-doc.jar exists
) else (
    set FAILED=%FAILED% ngs-doc.jar doesn't exist;
)

if exist %3\ngs-java\ngs-src.jar (
    rem ngs-src.jar exists
) else (
    set FAILED=%FAILED% ngs-src.jar doesn't exist;
)

if "%FAILED%" NEQ "" (
    echo "Failed: %FAILED%"
    exit /B 1
)

echo Ngs tarball smoke test successful
echo.

rem this is needed to expand variables inside the loop, e/g/ !VERSION_OPTION!
setlocal enabledelayedexpansion

set TOOLS=

rem list all tools; vdb-passwd is obsolete but still in the package
for /f %%F in ('dir /A:-D /B %1') do if "%%F" NEQ "vdb-passwd.exe" ( call set TOOLS=%%TOOLS%% %%F )

cd %1
set VERSION_CHECKER=%2
set VERSION=%4

echo Smoke testing %VERSION% toolkit tarball ...

for %%t in ( %TOOLS% ) do (
    call :RunTool %%t -h
    if errorlevel 1 ( call set FAILED=%%FAILED%% %%t -h ; )
)

for %%t in ( %TOOLS% ) do (
    rem All tools are supposed to respond to -V and --version, yet some respond only to --version, or -version, or nothing at all
    set VERSION_OPTION=-V
    if "%%t" EQU "blastn_vdb.exe"       ( set VERSION_OPTION=-version )
    if "%%t" EQU "sra-blastn.exe"       ( set VERSION_OPTION=-version )
    if "%%t" EQU "sra-tblastn.exe"      ( set VERSION_OPTION=-version )
    if "%%t" EQU "tblastn_vdb.exe"      ( set VERSION_OPTION=-version )
    if "%%t" EQU "dump-ref-fasta.exe"   ( set VERSION_OPTION=--version )
    echo %%t !VERSION_OPTION!
    start /b /wait %%t !VERSION_OPTION! | perl -w %VERSION_CHECKER% %VERSION% 2>&1
    if errorlevel 1 ( call set FAILED=%%FAILED%% %%t !VERSION_OPTION!; )
)

rem run some key tools, check return codes
call :RunTool test-sra
call :RunTool vdb-config
call :RunTool prefetch SRR002749
call :RunTool vdb-dump SRR000001 -R 1
call :RunTool fastq-dump SRR002749 -fasta -Z
call :RunTool sam-dump SRR002749
call :RunTool sra-pileup SRR619505 --quiet

if "%FAILED%" NEQ "" (
    echo "Failed: %FAILED%"
    exit /B 1
)

echo Toolkit tarball smoke test successful

echo.

set JAR=..\GenomeAnalysisTK.jar

echo Smoke testing %JAR% ...

set PWD=%CD%

set LOG=-Dvdb.log=FINEST
set LOG=

set ARGS=-Dvdb.System.loadLibrary=1 -Duser.home=%PWD%

set cmd=java %LOG% %ARGS% -cp %JAR% org.broadinstitute.gatk.engine.CommandLineGATK -T UnifiedGenotyper -I SRR835775 -R SRR835775 -L NC_000020.10:61000001-61010000 -o ..\chr20.SRR835775.vcf
echo %cmd%
%cmd% 2> NUL
if '%errorlevel%'=='1' goto skip
set FAILED=%FAILED% GenomeAnalysisTK.jar with disabled smart dll search;
:skip

set ARGS=-Duser.home=%PWD%

set cmd=java %LOG% %ARGS% -cp %JAR% org.broadinstitute.gatk.engine.CommandLineGATK -T UnifiedGenotyper -I SRR835775 -R SRR835775 -L NC_000020.10:61000001-61010000 -o ..\chr20.SRR835775.vcf
echo %cmd%
%cmd% > NUL 2>&1
if errorlevel 1 ( call set FAILED=%%FAILED%% GenomeAnalysisTK.jar; )

if "%FAILED%" NEQ "" (
    echo "Failed: %FAILED%"
    exit /B 1
)

echo %JAR% smoke test successful

exit /B 0

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
rem execute a tool and report if fails
rem to report, add the command line to the gloabl FAILED
:RunTool

echo %*
start /b /wait /c %* >NUL 2>&1
if errorlevel 1 ( call set FAILED=%%FAILED%% %*; )

exit /B %ERRORLEVEL%