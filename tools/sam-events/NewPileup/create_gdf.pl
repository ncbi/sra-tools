#!/usr/bin/perl

while(<>){
	chop;
	($na,$cnt1,$cnt2,$lbl,$links)=split;
	if($links){
		$nodes{$na}=$lbl;
		$edges{$na}=$links;
	}
}
print "nodedef>name INTEGER,label VARCHAR\n";
foreach $na (sort { $a <=> $b } keys nodes){
	print "$na,$nodes{$na}\n";
}
print "edgedef>node1 INTEGER,node2 INTEGER, weight INTEGER\n";
foreach $na (sort { $a <=> $b } keys edges){
        foreach $nbs (split(/;/,$edges{$na})){
            ($nb,$weight)=split(/:/,$nbs);
            print "$na,$nb,$weight\n";
        }
}
