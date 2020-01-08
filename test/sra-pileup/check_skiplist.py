#!/usr/bin/env python

import subprocess
import sys
import os.path

TOOL = sys.argv [ 1 ]

def check_if_tool_exits( tool ) :
    if not os.path.exists ( tool ):
        print ( "\nERROR: Can not find tool : '" + tool + "'\n" )
        exit ( 1 )

def run_tool( tool, args ) :
    a = [ tool ]
    for arg in args :
        a.append( arg )
    p = subprocess.Popen ( a, stdout = subprocess.PIPE, stderr = subprocess.PIPE )
    res  = "".join( chr( x ) for x in p.stdout.read() )
    if p.wait() != 0 :
        print ( "error executing tool" )
        exit( 1 )
    return res

check_if_tool_exits( TOOL )

REFNAME1 = "chr1"
REFNAME2 = "NC_000067.5"
RANGE1 = "3002426-3002426"
RANGE2 = "3002780-3002780"
SLICE1a = REFNAME1 + ":" + RANGE1
SLICE2a = REFNAME1 + ":" + RANGE2
SLICE1b = REFNAME2 + ":" + RANGE1
SLICE2b = REFNAME2 + ":" + RANGE2
ACCESSION = "SRR5486177"

print( "running step 1" )
out1 = run_tool( TOOL, [ ACCESSION, "-r", SLICE1a, "-r", SLICE2a ] )

print( "running step 2" )
out2 = run_tool( TOOL, [ ACCESSION, "-r", SLICE1b, "-r", SLICE2b ] )
if out1 != out2 :
    print ( "error comparison 1:" )
    print ( out1 )
    print ( "vs:" )
    print ( out2 )
    exit( 1 )

print( "running step 3" )
out2 = run_tool( TOOL, [ ACCESSION, "-r", SLICE1a, "-r", SLICE2a, "--merge-dist", "0" ] )
if out1 != out2 :
    print ( "error comparison 2:" )
    print ( out1 )
    print ( "vs:" )
    print ( out2 )
    exit( 1 )

print( "running step 4" )
out2 = run_tool( TOOL, [ ACCESSION, "-r", SLICE1b, "-r", SLICE2b, "--merge-dist", "0" ] )
if out1 != out2 :
    print ( "error comparison 3:" )
    print ( out1 )
    print ( "vs:" )
    print ( out2 )
    exit( 1 )

print( "running step 5" )
out2 = run_tool( TOOL, [ ACCESSION, "-r", SLICE1a, ] ) + run_tool( TOOL, [ ACCESSION, "-r", SLICE2a, ] )
if out1 != out2 :
    print ( "error comparison 4:" )
    print ( out1 )
    print ( "vs:" )
    print ( out2 )
    exit( 1 )

print( "running step 6" )
out2 = run_tool( TOOL, [ ACCESSION, "-r", SLICE1b, ] ) + run_tool( TOOL, [ ACCESSION, "-r", SLICE2b, ] )
if out1 != out2 :
    print ( "error comparison 5:" )
    print ( out1 )
    print ( "vs:" )
    print ( out2 )
    exit( 1 )

print ( "[" + os.path.basename ( __file__ ) + "] test passed for tool '" + TOOL + "'" )
exit( 0 )

