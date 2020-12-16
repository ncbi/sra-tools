#!/usr/local/bin/perl -w
use strict;
use Cwd qw(cwd);
my ($O, $cmd);
my $ACC = 'ERR2950949';

my $ALL = 1;

my $DEBUG = '';
#$DEBUG = '-+ VFS';

my $PRF_DEBUG = ''; # '-vv';

my $VERBOSE = 0;
#$VERBOSE = 1; # print what's executed'
#$VERBOSE = 2; # print commands
#$VERBOSE = 3; # print command output

`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"'               >  k`;

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
`echo '/repository/site/disabled = "true"' >> k`;

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
`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"'               >  k`;
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
