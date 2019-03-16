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
