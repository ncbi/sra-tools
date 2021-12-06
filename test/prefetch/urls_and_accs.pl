#$VERBOSE = 1;

($DIRTOTEST, $BINDIR) = @ARGV;

`mkdir -p tmp   tmp2`         ; die if $?;
`rm   -fr tmp/* tmp2/* *.lock`; die if $?;

`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"' > tmp/t.kfg`;
die if $?;

print "prefetch URL-1 when there is no kfg\n";
`rm -f wiki`; die if $?;
$URL = 'https://github.com/ncbi/ngs/wiki';
$CWD = `pwd`; die if $?; chomp $CWD;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $URL";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die 'Is there DIRTOTEST?' if $?;
`rm wiki`          ; die if $?;

print "prefetch URL-2/2 when there is no kfg\n";
`rm -f index.html`; die if $?;
$URL = 'https://github.com/ncbi/';
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $URL";
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
    "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch http://$INT/";
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
       "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $HTTP_URL";
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
 $CMD =
       "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $HTTP_URL";
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
 $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $URL";
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
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $URL";
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
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $URL";
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

$SRAC = 'SRR053325';

$SRR=`NCBI_SETTINGS=/ $BINDIR/srapath $SRAC`;
die 'Is there BINDIR?' if $?;
chomp $SRR;

print "prefetch $SRR when there is no kfg\n";
`rm -fr $SRAC`; die if $?;
$CMD = "NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y VDB_CONFIG=$CWD/tmp " .
       "$DIRTOTEST/prefetch $SRR";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`        ; die if $?;
`rm    $SRAC/SRR053325.sra`; die if $?;
`rmdir $SRAC`              ; die if $?;

print "SRR download when user repository is configured\n";
`echo '$PUBLIC/apps/sra/volumes/sraFlat = "sra"' >> tmp/t.kfg`; die if $?;
`rm -f $CWD/tmp/sra/$SRAC.sra`; die if $?;
$CMD = "ENV_VAR_LOG_HTTP_RETRY=1 NCBI_SETTINGS=/ NCBI_VDB_RELIABLE=y " .
       "VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $SRR";
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
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $REFSEQ";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm $REFSEQC`      ; die if $?;

print "REFSEQ HTTP download when user repository is configured\n";
`echo '$PUBLIC/apps/refseq/volumes/refseq = "refseq"' >> tmp/t.kfg`; die if $?;
`rm -f tmp/refseq/$REFSEQC`                                        ; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $REFSEQ";
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

print "$KMER HTTP download when there is no kfg should fail\n";
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $KMER";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die unless $?;

`echo '$PUBLIC/apps/nakmer/volumes/nakmerFlat = "nannot"' >> tmp/t.kfg`;
die if $?;

print "NANNOT download when user repository is configured\n";
`rm -f tmp/nannot/$KMERC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $KMER";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`   ; die if $?;
`ls tmp/nannot/$KMERC`; die if $?;

print "Running NANNOT prefetch second time finds previous download\n";
$CMD .= " 2>&1 | grep \"found local\"";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`      ; die if $?;
`rm tmp/nannot/$KMERC`; die if $?;

$WGSC = 'AFVF01.1';
$WGS  =                 "$SRA/traces/wgs03/WGS/AF/VF/$WGSC";
$WGSF = "$SRAF:/data/sracloud/traces/wgs03/WGS/AF/VF/$WGSC";

print "prefetch $WGS when there is no kfg\n";
`rm -f $WGSC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $WGS";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm $WGSC`         ; die if $?;

print "WGS HTTP download when user repository is configured\n";
`echo '$PUBLIC/apps/wgs/volumes/wgsFlat = "wgs"' >> tmp/t.kfg`; die if $?;
`rm -f tmp/wgs/$WGSC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $WGS";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`; die if $?;
`rm tmp/wgs/$WGSC`         ; die if $?;

`which ascp 2> /dev/null`;
unless ($?) {   $HAVE_NCBI_ASCP = 1 unless `hostname` eq "iebdev21\n" }

if ($HAVE_NCBI_ASCP) {
 print "REFSEQ FASP download when user repository is configured\n";
 `rm -f tmp/refseq/$REFSEQC`; die if $?;
 $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $REFSEQF";
 print "$CMD\n" if $VERBOSE;
 `$CMD 2> /dev/null`     ; die if $?;
 `rm tmp/refseq/$REFSEQC`; die if $?;
} else {
    print "download of $REFSEQF when ascp is not found is disabled\n";
}

if ($HAVE_NCBI_ASCP) {
   print "NANNOT FASP download when user repository is configured\n";
   `rm -f tmp/nannot/$KMERC`; die if $?;
   $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $KMERF";
   print "$CMD\n" if $VERBOSE;
   `$CMD 2> /dev/null`   ; die if $?;
   `rm tmp/nannot/$KMERC`; die if $?;
} else {
    print "download of $KMERF when ascp is not found is disabled\n";
}

if ($HAVE_NCBI_ASCP) {
    print "WGS FASP download when user repository is configured\n";
    `rm -f tmp/wgs/$WGSC`; die if $?;
    $CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $WGSF";
    print "$CMD\n" if $VERBOSE;
    `$CMD 2> /dev/null`   ; die if $?;
    `rm tmp/wgs/$WGSC`; die if $?;
} else {
    print "download of $WGSF when ascp is not found is disabled\n";
}

$SDL = 'https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve';

`echo 'repository/remote/main/SDL.2/resolver-cgi = "$SDL"' >> tmp/t.kfg`;
die if $?;

print "SRR accession download\n";
`rm -f tmp/sra/$SRAC.sra`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $SRAC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/sra/$SRAC.sra`; die if $?;

print "prefetch run with refseqs\n";
$ACC = 'SRR619505';
`rm -fr $ACC tmp/sra/$ACC.sra tmp/refseq/NC_000005.8`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $ACC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`        ; die if $?;
`rm tmp/sra/$ACC.sra`      ; die if $?;
`rm tmp/refseq/NC_000005.8`; die if $?;

print "REFSEQ accession download\n";
`rm -f tmp/refseq/$REFSEQC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $REFSEQC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`     ; die if $?;
`rm tmp/refseq/$REFSEQC`; die if $?;

print "NANNOT accession download\n";
`rm -f tmp/nannot/$KMERC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $KMERC";
#print `$CMD 2>&1`   ; die if $?;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/nannot/$KMERC`; die if $?;

print "WGS accession download\n";
$ACC = 'AFVF01';
`rm -f $ACC tmp/wgs/$ACC`; die if $?;
$CMD = "NCBI_SETTINGS=/ VDB_CONFIG=$CWD/tmp $DIRTOTEST/prefetch $ACC";
print "$CMD\n" if $VERBOSE;
`$CMD 2> /dev/null`   ; die if $?;
`rm tmp/wgs/$ACC`; die if $?;

`ascp -h > /dev/null`;
if ($?) {
    print "prefetch ASCP URL\n";
    $CMD = "$DIRTOTEST/prefetch $REFSEQF";
    print "$CMD\n" if $VERBOSE==0;
    `$CMD 2> /dev/null`;
    unless ($?) {
        die 'prefetch ASCP URL when ascp is not found has to fail'
    }
} else {
    print "prefetch ASCP URL when ascp is found is skipped\n"
}

`rm -rf tmp*`; die if $?;
