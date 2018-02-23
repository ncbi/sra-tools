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

rem this is needed to expand variables inside the loop, e/g/ !VERSION_OPTION!
setlocal enabledelayedexpansion

set TOOLS=

rem list all tools; vdb-passwd is obsolete but still in the package
for /f %%F in ('dir /A:-D /B %1') do if "%%F" NEQ "vdb-passwd.exe" ( call set TOOLS=%%TOOLS%% %%F )

cd %1
set VERSION_CHECKER=%2
set VERSION=%3

set FAILED=

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

echo "Tarballs test successful"
exit /B 0

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
rem execute a tool and report if fails
rem to report, add the command line to the gloabl FAILED
:RunTool

echo %*
start /b /wait /c %* >NUL 2>&1
if errorlevel 1 ( call set FAILED=%%FAILED%% %*; )

exit /B %ERRORLEVEL%