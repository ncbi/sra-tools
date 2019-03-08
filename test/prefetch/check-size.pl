use strict;

die if ( $#ARGV < 0 );
my $s = $ARGV [ 0 ];

$_ = `wc -c tmp/sra/SRR053325.sra | cut -d" " -f1`;
chomp;

my $e = 0;
#if ( $_ > 31681 )
if ( $_ > $s )
{   $e = 0 }
else {
    print "tmp/sra/SRR053325.sra is truncated\n";
    $e = 1
}

exit $e
