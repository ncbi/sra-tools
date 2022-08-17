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

#$VERBOSE = 1;

($DIRTOTEST, $BINDIR, $PREFETCH) = @ARGV;

`mkdir -p tmp   tmp2`         ; die if $?;
`rm   -fr tmp/* tmp2/* *.lock`; die if $?;

`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg`;
die if $?;

print "prefetch URL-1 when there is no kfg\n";
`rm -f wiki`; die if $?;
$URL = 'https://github.com/ncbi/ngs/wiki';
$CWD = `pwd`; die if $?; chomp $CWD;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $URL";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die 'Is there DIRTOTEST?' if $?;
`rm wiki`          ; die if $?;

print "prefetch URL-2/2 when there is no kfg\n";
`rm -f index.html`; die if $?;
$URL = 'https://github.com/ncbi/';
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $URL";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm index.html`    ; die if $?;

print "prefetch URL-2/1 when there is no kfg\n";
$INT = 'intranet';
`ping -c1 $INT > /dev/null`;
$NCBI = $? == 0;
if ($NCBI) {
    `rm -f index.html`; die if $?;
    $CMD =
        "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH http://$INT/";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`; die if $?;
    `rm index.html`    ; die if $?;
}

$HTTPFILE = 'contact.shtml';
$HTTP_URL = "https://test.ncbi.nlm.nih.gov/home/about/$HTTPFILE";

print "prefetch URL-2/3 when there is no kfg\n";
if ($NCBI) {
    `rm -f $HTTPFILE`; die if $?;
    $CMD =
       "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $HTTP_URL";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`; die if $?;
    `rm $HTTPFILE`    ; die if $?;
}

`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg`;
die if $?;

$PUBLIC = '/repository/user/main/public';
`echo '$PUBLIC/apps/file/volumes/flat = "files"' >> tmp/t.kfg`; die if $?;
`echo '$PUBLIC/root = "$CWD/tmp"'                >> tmp/t.kfg`; die if $?;

print "HTTP file download when user repository is configured\n";
if ($NCBI) {
    `rm -f $CWD/tmp2/$HTTPFILE`; die if $?;
    chdir "$CWD/tmp2" or die;
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`; die if $?;
    `ls $HTTPFILE`     ; die if $?;
    chdir $CWD        or die;
}

print "Running prefetch file second time finds previous download\n";
if ($NCBI) {
    chdir "$CWD/tmp2" or die;
    $CMD .= " 2>&1 | grep \"found local\"";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`; die if $?;
    `rm $HTTPFILE`    ; die if $?;
    chdir $CWD       or die;
}

print "HTTP dir download when user repository is configured\n";
if ($NCBI) {
    `rm -f $CWD/tmp2/index.html`; die if $?;
    chdir "$CWD/tmp2" or die;
    $URL = 'http://intranet/';
    $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $URL";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`; die if $?;
    `ls index.html`    ; die if $?;
    chdir $CWD        or die;
}

print "Running prefetch dir second time finds previous download\n";
if ($NCBI) {
    chdir "$CWD/tmp2" or die;
    $CMD .= " 2>&1 | grep \"found local\"";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`; die if $?;
    `rm index.html`    ; die if $?;
    chdir $CWD        or die;
}

print "URL download when user repository is configured\n";
chdir "$CWD/tmp2" or die;
`rm -f wiki`; die if $?;
$URL = 'https://github.com/ncbi/ngs/wiki';
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $URL";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`  ; die if $?;
`ls wiki`; die if $?;
chdir $CWD          or die;

print "Running prefetch URL second time finds previous download\n";
chdir "$CWD/tmp2" or die;
$CMD .= " 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`  ; die if $?;
`rm wiki`; die if $?;
chdir $CWD          or die;

print "URL/ download when user repository is configured\n";
chdir "$CWD/tmp2" or die;
`rm -f index.html`; die if $?;
$URL = 'https://github.com/ncbi/';
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $URL";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`ls index.html`    ; die if $?;
chdir $CWD        or die;

print "Running prefetch URL/ second time finds previous download\n";
chdir "$CWD/tmp2" or die;
$CMD .= " 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm index.html`    ; die if $?;
chdir $CWD        or die;

`echo '/libs/cloud/report_instance_identity = "false"' > tmp.mkfg`;
die if $?;

$SRAC = 'SRR053325';

$SRR=`NCBI_SETTINGS=tmp.mkfg $BINDIR/srapath $SRAC`;
die 'Is there BINDIR?' if $?;
chomp $SRR;

print "prefetch $SRR when there is no kfg\n";
`rm -fr $SRAC`; die if $?;
$CMD = "NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/$PREFETCH $SRR";
print "$CMD\n" if $VERBOSE;
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`        ; die if $?;
`rm    $SRAC/SRR053325.sra`; die if $?;
`rmdir $SRAC`              ; die if $?;

print "SRR download when user repository is configured\n";
`echo '$PUBLIC/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg`; die if $?;
`rm -f $CWD/tmp/sra/$SRAC.sra`; die if $?;
$CMD = "ENV_VAR_LOG_HTTP_RETRY=1 $CMD";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`    ; die if $?;
`ls $CWD/tmp/sra/$SRAC.sra`; die if $?;

print "Running prefetch SRR second time finds previous download\n";
$CMD .= " 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`        ; die if $?;
`rm $CWD/tmp/sra/$SRAC.sra`; die if $?;

$SRA   =       'https://sra-download.ncbi.nlm.nih.gov';
$SRAF  = 'fasp://dbtest@sra-download.ncbi.nlm.nih.gov';
$REFSEQC = 'KC702174.1';
$REFSEQ  =                "$SRA/traces/refseq/$REFSEQC";
$REFSEQF = "$SRAF:data/sracloud/traces/refseq/$REFSEQC";

print "prefetch $REFSEQ when there is no kfg\n";
`rm -f $REFSEQC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $REFSEQ";
print "$CMD\n" if $VERBOSE;
#print `$CMD 2>&1`; die if $?;
`$CMD 2> /dev/null`; die if $?;
`rm $REFSEQC`      ; die if $?;

print "REFSEQ HTTP download when user repository is configured\n";
`echo '$PUBLIC/apps/refseq/volumes/refseq = "refseq"' >> tmp/t.kfg`; die if $?;
`rm -f tmp/refseq/$REFSEQC`                                        ; die if $?;
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`     ; die if $?;
`ls tmp/refseq/$REFSEQC`; die if $?;

print "Running prefetch REFSEQ second time finds previous download\n";
$CMD .= " 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`      ; die if $?;
`rm tmp/refseq/$REFSEQC`; die if $?;

$KMERC = 'GCA_000390265.1_R';
$KMER  =                "$SRA/traces/nannot01/kmer/000/390/$KMERC";
$KMERF = "$SRAF:data/sracloud/traces/nannot01/kmer/000/390/$KMERC";

print "$KMER HTTP download when there is no kfg\n";
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $KMER";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`;
die "$KMER HTTP download when there is no kfg has to fail" unless $?;

`echo '$PUBLIC/apps/nakmer/volumes/nakmerFlat = "nannot"' >> tmp/t.kfg`;
die if $?;

print "KMER download when user repository is configured\n";
`rm -f tmp/nannot/$KMERC`; die if $?;
print "$CMD\n" if $VERBOSE;
#print `$CMD 2>&1`   ; die if $?;
`$CMD 2> /dev/null`   ; die if $?;
`ls tmp/nannot/$KMERC`; die if $?;

print "Running KMER prefetch second time finds previous download\n";
$CMD .= " 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`      ; die if $?;
`rm tmp/nannot/$KMERC`; die if $?;

$NANTC = 'NA000000007.1';
$NANT  = "$SRA/traces/nannot01/000/000/$NANTC";

print "$NANT download when there is no kfg\n";
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $NANT";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`;
die "$NANT download when there is no kfg has to fail" unless $?;

`echo '$PUBLIC/apps/nannot/volumes/nannotFlat = "nannot"' >> tmp/t.kfg`;
die if $?;

print "$NANT download when user repository is configured\n";
`rm -f tmp/nannot/$NANTC`; die if $?;
print "$CMD\n" if $VERBOSE;
#print `$CMD 2>&1`   ; die if $?;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/nannot/$NANTC`; die if $?;

$WGSC = 'AFVF01.1';
$WGS  =                 "$SRA/traces/wgs03/WGS/AF/VF/$WGSC";
$WGSF = "$SRAF:/data/sracloud/traces/wgs03/WGS/AF/VF/$WGSC";

print "prefetch $WGS when there is no kfg\n";
`rm -f $WGSC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $WGS";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm $WGSC`         ; die if $?;

print "WGS HTTP download when user repository is configured\n";
`echo '$PUBLIC/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/t.kfg`; die if $?;
`rm -f tmp/wgs/$WGSC`; die if $?;
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm tmp/wgs/$WGSC`         ; die if $?;

`which ascp 2> /dev/null`;
unless ($?)
{   $HAVE_NCBI_ASCP = 1 unless `hostname` eq "iebdev21\n" }

if ($HAVE_NCBI_ASCP) {
    print "REFSEQ FASP download when user repository is configured\n";
    `rm -f tmp/refseq/$REFSEQC`; die if $?;
    $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $REFSEQF";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`     ; die if $?;
    `rm tmp/refseq/$REFSEQC`; die if $?;
} else { print "download of $REFSEQF when ascp is not found is disabled\n" }

if ($HAVE_NCBI_ASCP) {
   print "NANNOT FASP download when user repository is configured\n";
   `rm -f tmp/nannot/$KMERC`; die if $?;
   $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $KMERF";
   print "$CMD\n" if $VERBOSE;
   `$CMD 2> /dev/null`   ; die if $?;
   `rm tmp/nannot/$KMERC`; die if $?;
} else { print "download of $KMERF when ascp is not found is disabled\n" }

if ($HAVE_NCBI_ASCP) {
    print "WGS FASP download when user repository is configured\n";
    `rm -f tmp/wgs/$WGSC`; die if $?;
    $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $WGSF";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`   ; die if $?;
    `rm tmp/wgs/$WGSC`; die if $?;
} else { print "download of $WGSF when ascp is not found is disabled\n" }

$SDL = 'https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve';

`echo 'repository/remote/main/SDL.2/resolver-cgi = "$SDL"' >> tmp/t.kfg`;
die if $?;

print "SRR accession download\n";
`rm -f tmp/sra/$SRAC.sra`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $SRAC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/sra/$SRAC.sra`; die if $?;

print "prefetch run with refseqs\n";
$ACC = 'SRR619505';
`rm -fr $ACC tmp/sra/$ACC.sra tmp/refseq/NC_000005.8`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $ACC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`        ; die if $?;
`rm tmp/sra/$ACC.sra`      ; die if $?;
`rm tmp/refseq/NC_000005.8`; die if $?;

print "REFSEQ accession download\n";
`rm -f tmp/refseq/$REFSEQC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $REFSEQC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`     ; die if $?;
`rm tmp/refseq/$REFSEQC`; die if $?;

print "NANNOT accession download\n";
`rm -f tmp/nannot/$KMERC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $KMERC";
#print `$CMD 2>&1`   ; die if $?;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/nannot/$KMERC`; die if $?;

print "WGS accession download\n";
$ACC = 'AFVF01';
`rm -f $ACC tmp/wgs/$ACC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/$PREFETCH $ACC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/wgs/$ACC`; die if $?;

`ascp -h > /dev/null`;
if ($?) {
    print "prefetch ASCP URL\n";
    $CMD = "$DIRTOTEST/$PREFETCH $REFSEQF -fy";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`;
    unless ($?) { die 'prefetch ASCP URL when ascp is not found has to fail' }
} else { print "prefetch ASCP URL when ascp is found is skipped\n" }

if ($HAVE_NCBI_ASCP) {
    print "prefetch ASCP <BAD SOURCE>\n";
    $CMD = "$BINDIR/prefetch fasp://anonftp@ftp.ncbi.nlm.nih.gov:100MB";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`;
    unless ($?)
    {   die 'prefetch <FASP> when FASP source is not found has to fail' }
} else { print "prefetch ASCP <BAD SOURCE> when ascp is found is skipped\n" }

`rm -rf tmp*`; die if $?;
