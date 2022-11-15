#!/usr/bin/perl -w

use strict;

use File::Temp   "tempdir";

my $tmp = tempdir ( "phgvXXXX", CLEANUP => 1 );
$ENV{VDB_CONFIG}=$tmp;
$ENV{NCBI_SETTINGS}="$tmp/u.mkfg";

`vdb-config -s foo=bar`;
die "vdb-config exited with " . ( $? >> 8 ) if ( $? );
