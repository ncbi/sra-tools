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

use strict;

die if ( $#ARGV < 0 );
my $s = $ARGV [ 0 ];

#print 'wc -c tmp/sra/SRR053325.sra | cut -d" " -f1' . "\n";
$_ = `wc -c tmp/sra/SRR053325.sra`;
chomp;
@_ = split ( /\s+/ );
die if ( $#_ <= 0 );
$_ = $_ [ $#_ - 1 ];

#print "size of tmp/sra/SRR053325.sra is $_\n";
my $e = 0;
#if ( $_ > 31681 )
if ( $_ > $s )
{   $e = 0 }
else {
    print "tmp/sra/SRR053325.sra is truncated\n";
    $e = 1
}

exit $e
