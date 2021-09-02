$D = ''; # '-+VFS';

$a = 'SRR053325';

`rm -fr $a` ; die if $?;

$c = "prefetch $a $D"; # print "$c\n";
$o = `$c` ; die if $?; # print "$o";
die unless -f "$a/$a.sra";

$o = `vdb-dump -R1 -CREAD $a/` ; die if $?; # print "$o";

`rm -r $a` ; die if $?;
