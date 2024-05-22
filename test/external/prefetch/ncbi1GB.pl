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

($DIRTOTEST, $PREFETCH, $verbose) = @ARGV;
my $VERBOSE = $verbose + 0;
#$VERBOSE = 1; # print what's executed'
#$VERBOSE = 2; # print commands
#$VERBOSE = 3; # print command output

`which ascp 2> /dev/null`;
unless ($?) {
    unless (`hostname`  =~ /v21/) {
        $FOUND = 1;

        print "FASP download: ncbi/1GB\n";

        $CWD = `pwd`; die if $?; chomp $CWD;

        `rm -fr tmp/*`; die if $?;
        `mkdir -p tmp`; die if $?;
        chdir 'tmp'  or die;

        `rm -f 1GB`; die if $?;

        $CMD = "NCBI_SETTINGS=/ " .
               "$DIRTOTEST/$PREFETCH fasp://anonftp\@ftp.ncbi.nlm.nih.gov:1GB";
        print "$CMD\n" if $VERBOSE > 1;
        $O = `$CMD`; die 'Is there DIRTOTEST?' if $?;
        print $O if $VERBOSE > 2;
        `rm 1GB`; die if $?;

        chdir $CWD or die;

        `rmdir tmp`; die if $?;
    }
}

print "ascp download when ascp is not found is disabled\n" unless $FOUND;

