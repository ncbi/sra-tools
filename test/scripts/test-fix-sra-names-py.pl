`which python3`;
if ( $? != 0 ) {
    print "python3 not found: exiting test-fix-sra-names-py.pl\n";
    exit 0;
}

$acc = 'SRR8639211';
`rm -fr $acc` ; die if $?;

# 1. do not fail if directory does not exist
`../../scripts/fix-sra-names.py $acc` ; die if $?;

# 2. single run file
`mkdir $acc` ; die if $?;
`touch $acc/$acc.1` ; die if $?;

# 3. fail if run without arguments
`../../scripts/fix-sra-names.py 2> /dev/null` ; die unless $?;

`../../scripts/fix-sra-names.py $acc` ; die if $?;
@d = `ls $acc | sort`;
die if $#d != 0;
die if $d[0] ne "$acc.sra\n";
`rm -fr $acc` ; die if $?;

# 4. run file & vdbcache
`mkdir $acc` ; die if $?;
`touch $acc/$acc.1` ; die if $?;
`touch $acc/$acc.vdbcache.1` ; die if $?;
`../../scripts/fix-sra-names.py $acc` ; die if $?;
@d = `ls $acc | sort`;
die if $#d != 1;
die if $d[0] ne "$acc.sra\n";
die if $d[1] ne "$acc.sra.vdbcache\n";
`rm -fr $acc` ; die if $?;

# 5. run file & vdbcache ; run has good name : do nothing
`mkdir $acc` ; die if $?;
`touch $acc/$acc.sra` ; die if $?;
`touch $acc/$acc.vdbcache.1` ; die if $?;
`../../scripts/fix-sra-names.py $acc` ; die if $?;
@d = `ls $acc | sort`;
die if $#d != 1;
die if $d[0] ne "$acc.sra\n";
die if $d[1] ne "$acc.vdbcache.1\n";
`rm -fr $acc` ; die if $?;

# 6. run file & vdbcache ; vdbcache has good name : do nothing
`mkdir $acc` ; die if $?;
`touch $acc/$acc.1` ; die if $?;
`touch $acc/$acc.sra.vdbcache` ; die if $?;
`../../scripts/fix-sra-names.py $acc` ; die if $?;
@d = `ls $acc | sort`;
die if $#d != 1;
die if $d[0] ne "$acc.1\n";
die if $d[1] ne "$acc.sra.vdbcache\n";
`rm -fr $acc` ; die if $?;
