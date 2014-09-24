#!/usr/bin/perl -w
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
use File::Path;

my $nargs = scalar @ARGV;

if ( $nargs < 2  )
{
    print( "\n-------------------------------------------------------------------------\n" );
    print( "USAGE: ref_grab.pl src position bases\n" );
    print( "src      ... absolute-path to a reference-table\n" );
    print( "position ... from what absolute positoin in the reference to grab\n" );
    print( "bases    ... how many bases to grab\n" );
    print( "-------------------------------------------------------------------------\n\n" );
}
else
{
    my $ref = $ARGV[ 0 ];
    my $cnt = $ARGV[ 1 ];
    my $len = 79;
    my ( $n_rows, $cmd, $row, $s, $line );

    #calculate how many rows have to be dumped...
    {
        use integer;
        $n_rows = ( $cnt / 5000 ) + 1;
    }

    #dump the sequence-name
    $cmd = "vdb-dump $ref -C SEQ_ID -f csv -R 1";
    open ( DUMP, "-|", "$cmd" ) or die "$cmd failed";
    while ( ( $row = <DUMP> ), defined( $row ) )
    {
        chomp( $row );
        print( ">$row\n" );
    }
    close ( DUMP );

    #dump the rows
    $cmd = "vdb-dump $ref -C READ -f tab -R 1-$n_rows";
    open ( DUMP, "-|", "$cmd" ) or die "$cmd failed";
    while ( ( $row = <DUMP> ), defined( $row ) )
    {
        chomp( $row );
        $s .= $row;
        while( length( $s ) >= $len )
        {
            $line = substr( $s, 0, $len, "" );
            print( "$line\n" );

        }
    }
    close ( DUMP );

    #dump the remainder of bases
    if ( length( $s ) > 0 )
    {
        print( "$s\n" );
    }
}