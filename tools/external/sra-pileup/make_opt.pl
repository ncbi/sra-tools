#!/usr/bin/perl -w

my $ref    =  $ARGV[ 0 ];
my $start  =  $ARGV[ 1 ];
my $length =  $ARGV[ 2 ];
my $space  =  $ARGV[ 3 ];
my $count  =  $ARGV[ 4 ];

for ( my $i = 0; $i < $count; $i++ )
{
    printf "-r %s:%d-%d\n", $ref, $start, $start + $length;
    $start += $space;
}
