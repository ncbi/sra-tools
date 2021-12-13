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

($DIRTOTEST) = @ARGV;

`which ascp 2> /dev/null`;
unless ($?) {
    unless (`hostname` eq "iebdev21\n") {
        $FOUND = 1;

        print "FASP download: ncbi/1GB\n";

        $CWD = `pwd`; die if $?; chomp $CWD;

        `rm -fr tmp/*`; die if $?;
        `mkdir -p tmp`; die if $?;
        chdir 'tmp'  or die;

        `echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > t.kfg`;
        die if $?;

        `rm -f 1GB`; die if $?;

        $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
               "$DIRTOTEST/prefetch fasp://anonftp\@ftp.ncbi.nlm.nih.gov:1GB";
        print "$CMD\n" if $VERBOSE;
        `$CMD 2> /dev/null`; die 'Is there DIRTOTEST?' if $?;
        `rm 1GB`; die if $?;

        chdir $CWD or die;

        `rm -r tmp`; die if $?;
    }
}

print "ascp download when ascp is not found is disabled\n" unless $FOUND;

