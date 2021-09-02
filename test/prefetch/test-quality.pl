$verbose = 1;
$verbose = 0;

$acc = 'SRR053325';

$cmd = "SRR053325=../data/no.json test-quality r $acc n";
print "$cmd\n" if $verbose;
`$cmd`; die if $?;

$cmd = "SRR053325=../data/full.json test-quality r $acc f";
print "$cmd\n" if $verbose;
`$cmd`; die if $?;

$cmd = "SRR053325=../data/dbl.json test-quality r $acc d";
print "$cmd\n" if $verbose;
`$cmd`; die if $?;

$cmd = "prefetch $acc";
`$cmd`; die if $?;

$cmd = "test-quality $acc/$acc.sra";
print "$cmd\n" if $verbose;
`$cmd`; die if $?;
