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
								### templates
                                my $t_positive = () = $hitlist =~ /\+/gi;
                                my $t_negative = () = $hitlist =~ /\-/gi;
								### reads
                                my $r_positive = () = $hitlist =~ /P/gi;
                                my $r_negative = () = $hitlist =~ /N/gi;
                                if(   ($r_positive > 1 && $r_negative > 1 && $t_positive > 1 && $t_negative > 1 )
                                    || $r_positive > 4 || $r_negative > 4 || $t_positive > 4 || $t_negative > 4){
                                        my %phout=();
                                        foreach $spotid (sort split /(?:P\+|P\-|N\+|N\-)/,$hitlist )
                                        {
                                                foreach $rid ( @ {$phasing_cache{$spotid}})
                                                {
                                                        if($rid!=$rowid){ $phout{$rid}++;}
                                                }
                                                #foreach  $rid (keys %phout){print "$rid\n"}

                                                push( @ {$phasing_cache{$spotid}},$rowid);
                                        }
                                        print("$rowid\t$t_positive\t$t_negative\t$r_positive\t$r_negative\t$a\t");
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
