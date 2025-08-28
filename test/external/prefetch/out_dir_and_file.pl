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

use Cwd qw(abs_path);

($DIRTOTEST, $BINDIR, $PREFETCH, $VERBOSE) = @ARGV;
$DIRTOTEST = abs_path($DIRTOTEST);
$BINDIR    = abs_path($BINDIR   );

`mkdir -p tmp tmp2`             ; die if $?;
`rm -fr tmp*/*`                 ; die if $?;
`echo  version 1.0     >  tmp/k`; die if $?;
`echo '0||SRR053325||' >> tmp/k`; die if $?;
`echo '$$end'          >> tmp/k`; die if $?;

$SDL = 'https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve';
`echo 'repository/remote/main/SDL.2/resolver-cgi = "$SDL"'  >  tmp/t.kfg`;
die if $?;

$CWD = `pwd`; die if $?; chomp $CWD;

$PUBLIC = '/repository/user/main/public';
`echo '$PUBLIC/apps/sra/volumes/sraFlat = "sra"'       >> tmp/t.kfg`; die if $?;
`echo '$PUBLIC/root = "$CWD/tmp"'                      >> tmp/t.kfg`; die if $?;
`echo '/repository/site/disabled = "true"'             >> tmp/t.kfg`; die if $?;

$SRAC = 'SRR053325';

print "PREFETCH ACCESSION TO SINGLE OUT-FILE\n";
`rm -f tmp-file`; die if $?;
$CMD = "NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH $SRAC -o tmp-file";
print "$CMD\n" if $VERBOSE;
`NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= $CMD`; die 'Is there DIRTOTEST?' if $?;
`rm tmp-file`; die if $?;

print "PREFETCH ACCESSION TO OUT-FILE INSIDE OF DIR\n";
`rm -f tmp3/dir/file`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH $SRAC -O / -o tmp3/dir/file";
print "$CMD\n" if $VERBOSE;
`NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= $CMD`; die if $?;
`rm tmp3/dir/file` ; die if $?;

`echo '/libs/cloud/report_instance_identity = "false"' > tmp.mkfg`;
die if $?;

$SRR = `NCBI_SETTINGS=tmp.mkfg $BINDIR/srapath $SRAC`;
die 'Is there BINDIR?' if $?;
chomp $SRR;

print "PREFETCH SRR HTTP URL TO OUT-FILE\n";
`rm -f tmp3/dir/file`; die if $?;
$CMD = "NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=$CWD/tmp " .
	   "$DIRTOTEST/$PREFETCH $SRR -O / -o tmp3/dir/file";
print "$CMD\n" if $VERBOSE;
`NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= $CMD`; die if $?;
`rm tmp3/dir/file` ; die if $?;

print "PREFETCH HTTP DIRECTORY URL TO OUT-FILE\n";
`rm -f tmp3/dir/file`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH https://github.com/ncbi/ -O / -o tmp3/dir/file";
print "$CMD\n" if $VERBOSE;
`NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= $CMD`; die if $?;
`rm tmp3/dir/file` ; die if $?;

print "PREFETCH HTTP FILE URL TO OUT-FILE\n";
`rm -f tmp3/dir/file`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
  "$DIRTOTEST/$PREFETCH https://github.com/ncbi/ngs/wiki -O / -o tmp3/dir/file";
print "$CMD\n" if $VERBOSE;
`NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= $CMD`; die if $?;
`rm tmp3/dir/file` ; die if $?;

print "downloading multiple items to file\n";
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH SRR045450 $SRAC -o tmp/o";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die unless $?;

print "PREFETCH MULTIPLE ITEMS\n";
`rm -fr SRR0* tmp/sra`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH SRR045450 $SRAC";
print "$CMD\n" if $VERBOSE;
`$CMD`; die if $?;
`rm tmp/sra/SRR045450.sra tmp/sra/$SRAC.sra`; die if $?;

print "PREFETCH SRR HTTP URL\n";
`rm -fr tmp/sra/$SRAC.sra`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $SRR";
print "$CMD\n" if $VERBOSE;
`$CMD`; die if $?;
`rm tmp/sra/$SRAC.sra`; die if $?;

print "PREFETCH HTTP DIRECTORY URL\n";
chdir 'tmp2'     or die;
`rm -f index.html`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH https://github.com/ncbi/";
print "$CMD\n" if $VERBOSE;
`$CMD`; die if $?;
`rm index.html`    ; die if $?;
chdir $CWD        or die;

print "PREFETCH HTTP FILE URL\n";
chdir 'tmp2'     or die;
`rm -f wiki`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH https://github.com/ncbi/ngs/wiki";
print "$CMD\n" if $VERBOSE;
`$CMD`      ; die if $?;
`rm wiki`   ; die if $?;
chdir $CWD or die;

print "PREFETCH ACCESSION TO OUT-DIR\n";
`rm -f tmp3/dir/$SRAC/$SRAC.sra`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH $SRAC -O tmp3/dir";
print "$CMD\n" if $VERBOSE;
`$CMD`; die if $?;
`rm tmp3/dir/$SRAC/$SRAC.sra`; die if $?;

print "PREFETCH SRR HTTP URL TO OUT-DIR\n";
`rm -f tmp3/dir/$SRAC/$SRAC.sra`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH $SRR -O tmp3/dir";
print "$CMD\n" if $VERBOSE;
`$CMD`; die if $?;
`rm tmp3/dir/$SRAC/$SRAC.sra`; die if $?;

print "PREFETCH HTTP DIRECTORY URL TO OUT-DIR\n";
`rm -f index.html tmp3/dir/index.html`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH https://github.com/ncbi/ -O tmp3/dir";
print "$CMD\n" if $VERBOSE;
`$CMD`     ; die if $?;
`rm tmp3/dir/index.html`; die if $?;

print "PREFETCH HTTP FILE URL TO OUT-DIR\n";
`rm -f wiki tmp3/dir/wiki`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH https://github.com/ncbi/ngs/wiki -O tmp3/dir";
print "$CMD\n" if $VERBOSE;
`$CMD`; die if $?;
`rm tmp3/dir/wiki` ; die if $?;

`rm -r tmp*`; die if $?;
