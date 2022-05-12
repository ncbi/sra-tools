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

($DIRTOTEST, $PREFETCH) = @ARGV;

#$VERBOSE = 1;

print "vdbcache download\n";

$CWD = `pwd`; die if $?; chomp $CWD;

$ACC = 'SRR6667190';

`rm -fr $ACC*`; die if $?;

`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > $ACC.kfg`;
die if $?;

print "download sra and vdbcache\n" if $VERBOSE;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD $DIRTOTEST/$PREFETCH $ACC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`        ; die 'Is there DIRTOTEST?' if $?;
`ls $ACC/$ACC.sra`         ; die if $?;
`ls $ACC/$ACC.sra.vdbcache`; die if $?;

print "second run of prefetch finds local\n" if $VERBOSE;
$CMDL = "$CMD 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;

print "download missed sra\n" if $VERBOSE;
`rm $ACC/$ACC.sra`; die if $?;
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`ls $ACC/$ACC.sra` ; die if $?;

print "download missed vdbcache\n" if $VERBOSE;
`rm $ACC/$ACC.sra.vdbcache`; die if $?;
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`         ; die if $?;
`ls $ACC/$ACC.sra.vdbcache` ; die if $?;

print "prefetch works when AD is empty\n" if $VERBOSE;
`rm $ACC/*`; die if $?;
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`         ; die if $?;
`ls $ACC/$ACC.sra` ; die if $?;
`ls $ACC/$ACC.sra.vdbcache` ; die if $?;

`rm -r $ACC*`; die if $?;
