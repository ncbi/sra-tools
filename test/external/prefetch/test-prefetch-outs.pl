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
# =============================================================================$

print "Verify prefetch output...\n";

($DIRTOTEST, $prefetch, $verbose) = @ARGV;
$PREFETCH = "$DIRTOTEST/$prefetch";

$VERBOSE = $verbose + 0;
#$VERBOSE = 1; # print what's executed'
#$VERBOSE = 2; # print commands
#$VERBOSE = 3; # print command output
#$VERBOSE = 4; # don't remove work files

$TMP = 'tmp';
`rm -fr $TMP`; die if $?;
`mkdir -p $TMP`; die if $?;
$CWD = `pwd`; die if $?; chomp $CWD;
chdir $TMP or die;

$ACC = 'SRR053325';

################################################################################

print "\n" if ($VERBOSE > 1);
print "prefetch prints to stdout\n" if $VERBOSE;
$CMD = "NCBI_SETTINGS=/ $PREFETCH $ACC > $ACC.out 2> $ACC.err";
print "$CMD\n" if $VERBOSE > 1;
$O = `$CMD`; die if $?;
die unless -d $ACC;
die unless -z "$ACC.err";
die unless -s "$ACC.out";
if ($VERBOSE > 2) { print `cat $ACC.out`; die if $? }
`rm -r $ACC*`; die if $?;

################################################################################

print "\n" if ($VERBOSE > 1);
print "prefetch --quiet is silent\n" if $VERBOSE;
$CMD = "NCBI_SETTINGS=/ $PREFETCH --quiet $ACC > $ACC.out 2> $ACC.err";
print "$CMD\n" if $VERBOSE > 1;
$O = `$CMD`; die if $?;
die unless -d $ACC;
die unless -z "$ACC.err";
die unless -z "$ACC.out";
`rm -r $ACC*`; die if $?;

################################################################################

print "\n" if ($VERBOSE > 1);
print "prefetch reports errors to contact SDL\n" if $VERBOSE;

`echo 'http/timeout/read = "1"' > k`; die if $?;

$CMD = "NCBI_SETTINGS=k $PREFETCH $ACC > $ACC.out 2> $ACC.err";
print "$CMD\n" if $VERBOSE > 1;
$O = `$CMD`; die unless $?;
die if -e $ACC;
die unless -s "$ACC.err";
die unless -s "$ACC.out";
if ($VERBOSE > 2) { print `cat $ACC*`; die if $? }
`rm -r $ACC.*`; die if $?;

################################################################################

print "\n" if ($VERBOSE > 1);
print "prefetch reports errors to contact SDL when --quiet\n" if $VERBOSE;

`echo 'http/timeout/read = "1"' > k`; die if $?;

$CMD = "NCBI_SETTINGS=k $PREFETCH $ACC --quiet > $ACC.out 2> $ACC.err";
print "$CMD\n" if $VERBOSE > 1;
$O = `$CMD`; die unless $?;
die if -e $ACC;
die unless -s "$ACC.err";
die unless -z "$ACC.out";
if ($VERBOSE > 2) { print `cat $ACC.err`; die if $? }
`rm $ACC.*`; die if $?;

################################################################################

`rm k`; die if $?;
chdir $CWD or die;
unless ($VERBOSE > 3) { `rmdir $TMP`; die if $? }
