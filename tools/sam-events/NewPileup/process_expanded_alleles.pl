#!/usr/bin/perl
my %allele_cache;
my $next_report_pos=0;
my $report_step=1000;
my $rowid=1;
my %phasing_cache;

while(<>)
{
        chop;
        ($spid_list,$q,$signature,$r)=split;
        if($signature){
                ($ref,$pos,$del,$allele)=split(/:/,$signature);
                if( $next_report_pos == 0) { $next_report_pos = $pos };
                if($pos > $next_report_pos +  $report_step){
                        flush_cache($next_report_pos);
                        $next_report_pos += $report_step;
                }
                $allele_cache{$pos}{$signature}.="$spid_list";
        }
}
close DUMP;
flush_cache(0);


sub flush_cache
{
        my $pp=shift @_;
        foreach $p (sort keys %allele_cache)
        {
                if($pp==0 || $p < $pp ){
                        foreach $a (keys %{$allele_cache{$p}}){
                                my $hitlist=$allele_cache{$p}{$a};
                                my $positive = () = $hitlist =~ /\+/gi;
                                my $negative = () = $hitlist =~ /\-/gi;
                                if(($positive > 1 && $negative > 1)|| $positive > 4 || $negative > 4 ){
                                        my %phout=();
                                        foreach $spotid (sort split(/[+-]/,$hitlist) )
                                        {
                                                foreach $rid ( @ {$phasing_cache{$spotid}})
                                                {
                                                        if($rid!=$rowid){ $phout{$rid}++;}
                                                }
                                                #foreach  $rid (keys %phout){print "$rid\n"}

                                                push( @ {$phasing_cache{$spotid}},$rowid);
                                        }
                                        print("$rowid\t$positive\t$negative\t$a\t");
                                        foreach  $rid (sort { $phout{$b} <=> $phout{$a} } keys %phout ) {
                                                print("$rid:$phout{$rid};");
                                        }
                                        print "\n";
                                        $rowid++;
                                }
                        }
                        if($pp!=0) {delete $allele_cache{$p};}
                }
        }
}
