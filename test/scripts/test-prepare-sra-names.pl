$acc = 'GSE118828';
`rm -fr $acc` ; die if $?;

`mkdir $acc` ; die if $?;
chdir $acc || die;

# 1. fail if run without arguments
`../../../scripts/prepare-sra-names.sh` ; die unless $?;

$acs = 'SRR7725681';

# 2. don't fail if accession files do not exist
`../../../scripts/prepare-sra-names.sh $acs` ; die if $?;

`touch $acs.1` ; die if $?;

# 3. don't fail if path argument is used
`../../../scripts/prepare-sra-names.sh ../$acp/$acs` ; die if $?;

# 4. single run file
`../../../scripts/prepare-sra-names.sh $acs` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$acs\n";

@d = `ls $acs | sort`;
die if $#d != 0;
die if $d[0] ne "$acs.1\n";

`rm -fr $acs` ; die if $?;

# 5. run file & vdbcache
`touch $acs.1` ; die if $?;
`touch $acs.vdbcache.1` ; die if $?;
`../../../scripts/prepare-sra-names.sh $acs` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$acs\n";

@d = `ls $acs | sort`;
die if $#d != 1;
die if $d[0] ne "$acs.1\n";
die if $d[1] ne "$acs.vdbcache.1\n";
`rm -fr $acs` ; die if $?;

# 6. DRR

$acd = 'DRR7725681';
`touch $acd.1` ; die if $?;

`../../../scripts/prepare-sra-names.sh $acd` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$acd\n";

@d = `ls $acd | sort`;
die if $#d != 0;
die if $d[0] ne "$acd.1\n";

`rm -fr $acd` ; die if $?;

# 7. ERR

$ace = 'ERR7725681';

`touch $ace.1` ; die if $?;
`touch $ace.vdbcache.1` ; die if $?;
`../../../scripts/prepare-sra-names.sh $ace` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$ace\n";

@d = `ls $ace | sort`;
die if $#d != 1;
die if $d[0] ne "$ace.1\n";
die if $d[1] ne "$ace.vdbcache.1\n";
`rm -fr $ace` ; die if $?;

# 7. non-run accession

$acp = 'SRS7725681';

`touch $acp.1` ; die if $?;
`../../../scripts/prepare-sra-names.sh $acp` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$acp.1\n";
`rm $acp.1` ; die if $?;

# 8. ACC dir exists
`mkdir $acs` ; die if $?;
`touch $acs.1` ; die if $?;
`../../../scripts/prepare-sra-names.sh $acs` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$acs\n";

@d = `ls $acs | sort`;
die if $#d != 0;
die if $d[0] ne "$acs.1\n";

`rm -fr $acs` ; die if $?;

# 9. other files
`touch $acs.foo` ; die if $?;
`../../../scripts/prepare-sra-names.sh $acs` ; die if $?;
@d = `ls | sort`;
die if $#d != 0;
die if $d[0] ne "$acs\n";

@d = `ls $acs | sort`;
die if $#d != 0;
die if $d[0] ne "$acs.foo\n";

`rm -fr $acs` ; die if $?;

`rm -fr $acs` ; die if $?;

chdir '..' || die;
`rmdir $acc` ; die if $?;
