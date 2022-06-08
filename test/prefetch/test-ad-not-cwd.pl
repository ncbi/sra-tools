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
# ==============================================================================

use Cwd "cwd";
my $root = cwd();

`prefetch -h`; die 'no prefetch' if $?; # make sure SRA toolkit is found

`rm -fr tmp`; die if $?;
mkdir 'tmp' || die;
mkdir 'tmp/out' || die;
mkdir 'tmp/kfg' || die;

################################## prepare kfg #################################
# site repo is always ignored
$K = "$root/tmp/kfg";
$k = "$K/k.kfg";
`echo '/LIBS/GUID = "8test002-6ab7-41b2-bfd0-prefetchpref"'  > $k`; die if $?;
`echo '/repository/site/disabled = "true"'                  >> $k`; die if $?;

# no remote
$rn = "$K/rn.mkfg";
`echo '/repository/remote/disabled = "true"' > $rn`; die if $?;

# user repo; no remote
$rnu = "$K/rnu.mkfg";
`echo '/repository/remote/disabled = "true"' > $rnu`; die if $?;
`echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' >> $rnu`; die if $?;

`echo '/repository/user/main/public/apps/sra/volumes/sraFlat = "sra"' >> $rnu`;
die if $?;

`echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> $rnu`;
die if $?;

`echo '/repository/user/main/public/root = "$root/tmp/r"' >> $rnu`; die if $?;

# remote repo
$s = 'https://www.ncbi.nlm.nih.gov/Traces/sdl/2/retrieve';
$ry = "$K/ry.mkfg";
`echo '/repository/remote/main/SDL.2/resolver-cgi = "$s"' > $ry`; die if $?;

# user and remote repos
$ryu = "$K/ryu.mkfg";
`echo '/repository/remote/main/SDL.2/resolver-cgi = "$s"' > $ryu`; die if $?;
`echo '/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"' >> $ryu`; die if $?;

`echo '/repository/user/main/public/apps/sra/volumes/sraFlat = "sra"' >> $ryu`;
die if $?;

`echo '/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"' >> $ryu`;
die if $?;

`echo '/repository/user/main/public/root = "$root/tmp/r"' >> $ryu`; die if $?;
################################# kfg prepared #################################

chdir "$root/tmp/out" || die;

$a = 'SRR619505';

`rm -fr $a`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-config -on`; die if $?;

# cannot find run if remote repo is disabled
`NCBI_SETTINGS=$rn VDB_CONFIG=$K srapath $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump $a 2> /dev/null`; die unless $?;

# prefetch into cwd/AD
`NCBI_SETTINGS=$ry VDB_CONFIG=$K prefetch $a 2> /dev/null`; die if $?;
`ls $a/$a.sra` ; die 'no $a.sra' if $?;
`ls $a/NC_000005.8` ; die 'no NC_000005.8' if $?;

# run in cwd/AD
`NCBI_SETTINGS=$rn VDB_CONFIG=$K srapath $a`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump -X 1 -Z $a 2> /dev/null`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump $a`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump -R1 -CREAD $a`; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die if $?;
`rm $a.fastq`; die if $?;
 
# remove refseq: expect failures
`rm $a/NC_000005.8` ; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump $a 2> /dev/null`; die unless $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump -R1 -CREAD $a 2>&1 | grep "can't open NC_000005.8"`; die if $?;

# prefetch refseq into cwd/AD
`NCBI_SETTINGS=$ry VDB_CONFIG=$K prefetch $a 2> /dev/null`; die if $?;
`ls $a/NC_000005.8` ; die 'no NC_000005.8' if $?;

`rm -fr $a`; die if $?;

# cannot find run if remote repo is disabled
`NCBI_SETTINGS=$rn VDB_CONFIG=$K srapath $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump $a 2> /dev/null`; die unless $?;

# prefetch into out-dir
`NCBI_SETTINGS=$ry VDB_CONFIG=$K prefetch $a -O Q 2> /dev/null`; die if $?;
`ls Q/$a/$a.sra` ; die 'no $a.sra' if $?;
`ls Q/$a/NC_000005.8` ; die 'no NC_000005.8' if $?;

# run in cwd/AD
chdir "$root/tmp/out/Q" || die;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K srapath $a`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump -X 1 -Z $a 2> /dev/null`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump $a`; die if $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump -R1 -CREAD $a`; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die if $?;
`rm $a.fastq`; die if $?;
 
chdir "$root/tmp" || die;

# access run via path to sra file
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump -X 1 -Z out/Q/$a/$a.sra 2> /dev/null`; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump out/Q/$a/$a.sra`; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump -R1 -CREAD out/Q/$a/$a.sra`;
die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump out/Q/$a/$a.sra 2> /dev/null`;
die if $?;
`rm $a.fastq`; die if $?;

# remove refseq: expect failures
`rm out/Q/$a/NC_000005.8` ; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K fastq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sam-dump $a 2> /dev/null`; die unless $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K vdb-dump -R1 -CREAD out/Q/$a/$a.sra 2>&1 | grep "can't open NC_000005.8"`; die if $?;

# prefetch refseq into out-dir
`NCBI_SETTINGS=$ry VDB_CONFIG=$K prefetch $a -O Q 2> /dev/null`; die if $?;
`ls Q/$a/NC_000005.8` ; die 'no NC_000005.8' if $?;

`rm -fr out r`; die if $?;

# cannot find run if remote repo is disabled
`NCBI_SETTINGS=$rn VDB_CONFIG=$K srapath $a 2> /dev/null`;
die unless $?;

# cannot find run if remote repo is disabled and user repo is empty
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K srapath $a 2> /dev/null`;
die unless $?;

`NCBI_SETTINGS=$rnu VDB_CONFIG=$K fastq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K sam-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K vdb-dump $a 2> /dev/null`; die unless $?;

`rm -rf Q`; die if $?;

# cannot find run if remote repo is disabled
$d = 'SRR341578';

`NCBI_SETTINGS=$rn VDB_CONFIG=$K sra-pileup --function ref Q/$d 2> /dev/null`;
die unless $?;

# prefetch into out-dir
`NCBI_SETTINGS=$ry VDB_CONFIG=$K prefetch $d -O Q 2> /dev/null`; die if $?;
`ls Q/$d/$d.sra` ; if ($?) { `ls Q/$d/$d.sralite`; die if $? }
`ls Q/$d/$d.sra.vdbcache` ; die 'no $d.sra.vdbcache' if $?;
`ls Q/$d/NC_011748.1` ; die 'no NC_011748.1' if $?;
`ls Q/$d/NC_011752.1` ; die 'no NC_011752.1' if $?;

# access run via path to AD
`NCBI_SETTINGS=$rn VDB_CONFIG=$K sra-pileup --function ref Q/$d`; die if $?;

# remove refseq: expect failures
`rm Q/$d/NC_011748.1` ; die if $?;
`rm Q/$d/$d.sra.vdbcache` ; die if $?;

`NCBI_SETTINGS=$rn VDB_CONFIG=$K sra-pileup --function ref Q/$d 2> /dev/null`;
die unless $?;

# prefetch refseq into out-dir
`NCBI_SETTINGS=$ry VDB_CONFIG=$K prefetch $d -O Q 2> /dev/null`; die if $?;
`ls Q/$d/NC_011748.1` ; die 'no NC_011748.1' if $?;
`ls Q/$d/$d.sra.vdbcache` ; die 'no $d.sra.vdbcache' if $?;

# prefetch into user repo
`NCBI_SETTINGS=$ryu VDB_CONFIG=$K prefetch $a 2> /dev/null`; die if $?;
`ls $root/tmp/r/sra/$a.sra` ; die 'no $a.sra' if $?;
`ls $root/tmp/r/refseq/NC_000005.8` ; die 'no NC_000005.8' if $?;

# run in user repo
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K srapath $a`; die if $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K fastq-dump -X 1 -Z $a 2> /dev/null`; die if $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K sam-dump $a`; die if $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K vdb-dump -R1 -CREAD $a`; die if $?;

`NCBI_SETTINGS=$rnu VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die if $?;
`rm $a.fastq`; die if $?;

# remove refseq: expect failures
`rm $root/tmp/r/refseq/NC_000005.8` ; die if $?;

`NCBI_SETTINGS=$rnu VDB_CONFIG=$K fastq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K fasterq-dump $a 2> /dev/null`; die unless $?;
`NCBI_SETTINGS=$rnu VDB_CONFIG=$K sam-dump $a 2> /dev/null`; die unless $?;

`NCBI_SETTINGS=$rnu VDB_CONFIG=$K vdb-dump -R1 -CREAD $a 2>&1 | grep "can't open NC_000005.8"`; die if $?;

# prefetch refseq into user repo
`NCBI_SETTINGS=$ryu VDB_CONFIG=$K prefetch $a 2> /dev/null`; die if $?;
`ls $root/tmp/r/refseq/NC_000005.8` ; die 'no NC_000005.8' if $?;

`rm -r $root/tmp` ; die if $?;
