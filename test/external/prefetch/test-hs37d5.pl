#!/usr/local/bin/perl -w

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
#  may be obtained by using this >oftware or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ==============================================================================

use strict;
use Cwd qw(cwd);

my ($verbose) = @ARGV;
$verbose = 0 unless ($verbose);

my ($O, $cmd);
my $ACC = 'ERR2950949';

my $ALL = 1;

my $DEBUG = '';
#$DEBUG = '-+ VFS';

my $PRF_DEBUG = ''; # '-vv';

my $VERBOSE = $verbose + 0;
#$VERBOSE = 1; # print what's executed'
#$VERBOSE = 2; # print commands
#$VERBOSE = 3; # print command output

`rm -fr ERR2950949`;

if ($ALL) {
########## use site repository (if exists) from default configuration ##########
print "########################################################\n" if($VERBOSE);
print "vdb-dump - use site repository\n" if ($VERBOSE);
$cmd = "vdb-dump $ACC -R 1489067 $DEBUG -CREAD";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);
}

if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "fastq-dump - use site repository\n" if ($VERBOSE);
$cmd = "fastq-dump $ACC -N 1489067 -X 1489067 $DEBUG -Z";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);
}

########## use remote repository ##########
`echo '/repository/site/disabled = "true"' > k`;

if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "vdb-dump - use remote repository\n" if ($VERBOSE);
$cmd = "vdb-dump $ACC -R 1489067 $DEBUG -CREAD";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);
}

if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "fastq-dump - use remote repository\n" if ($VERBOSE);
$cmd = "fastq-dump $ACC -N 1489067 -X 1489067 $DEBUG -Z";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);
}


########## prefetch to AD ##########
if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "prefetch to AD\n" if ($VERBOSE);
$cmd = "prefetch $ACC $DEBUG $PRF_DEBUG";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);

`echo '/repository/remote/disabled = "true"' >> k`;
}


if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "vdb-dump - use AD\n" if ($VERBOSE);
$cmd = "vdb-dump $ACC -R 1489067 $DEBUG -CREAD";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);
}

if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "fastq-dump - use AD repository\n" if ($VERBOSE);
$cmd = "fastq-dump $ACC -N 1489067 -X 1489067 $DEBUG -Z";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);
}

$O = `rm -rv $ACC`; die if ($?);
print $O if ($VERBOSE > 2);

########## prefetch to user repository ##########
my $cwd = cwd();
`echo '/repository/site/disabled = "true"'                                >> k`;
`echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"'>> k`;
`echo '/repository/user/main/public/apps/sra/volumes/sraFlat = "sra"     '>> k`;
`echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"'     >> k`;
`echo '/repository/user/main/public/root = "$cwd"\n'                      >> k`;

if ($ALL) {
print "########################################################\n" if($VERBOSE);
print "prefetch to user repository\n" if ($VERBOSE);
$cmd = "prefetch $ACC $DEBUG $PRF_DEBUG";
print "$cmd\n" if ($VERBOSE > 1);
$O = `$cmd 2>&1`;
print $O if ($VERBOSE > 2);
die if ($?);

`echo '/repository/remote/disabled = "true"' >>  k`;
}

if ($ALL) {
print "########################################################\n" if($VERBOSE);
$_ = `which srapath`; chomp;
$_ = readlink ( $_ );
$_ = `which $_`; chomp;
$_ = readlink($_);
$_ = `which $_`; chomp;
if (-l $_) {
    print "skipped(remote dsb): vdb-dump - use user repository\n" if ($VERBOSE);
} else {
    print "vdb-dump - use user repository\n" if ($VERBOSE);
    $cmd = "vdb-dump $ACC -R 1489067 $DEBUG -CREAD";
    print "$cmd\n" if ($VERBOSE > 1);
    $O = `$cmd 2>&1`;
    print $O if ($VERBOSE > 2);
    die if ($?);
}
}

if ($ALL) {
print "########################################################\n" if($VERBOSE);
$_ = `which srapath`; chomp;
$_ = readlink ( $_ );
$_ = `which $_`; chomp;
$_ = readlink($_);
$_ = `which $_`; chomp;
if (-l $_) {
    print "skipped(remote dsb): fastq-dump - use user repository\n" if ($VERBOSE);
} else {
    print "fastq-dump - use  user repository\n" if ($VERBOSE);
    $cmd = "fastq-dump $ACC -N 1489067 -X 1489067 $DEBUG -Z";
    print "$cmd\n" if ($VERBOSE > 1);
    $O = `$cmd 2>&1`;
    print $O if ($VERBOSE > 2);
    die if ($?);
}
}
