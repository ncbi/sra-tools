#!/usr/bin/perl
my %allele_cache;
$col_list="SEQ_SPOT_ID,SEQ_READ_ID,REF_ORIENTATION,REF_SEQ_ID,REF_POS,CIGAR_LONG,READ,READ_FILTER";
$row_range="100000-1100000";
$acc=$ARGV[0];
if($#ARGV>0){$row_range = $ARGV[1];}

$vdbdump="/panfs/traces01.be-md.ncbi.nlm.nih.gov/trace_software/toolkit/centos64/bin/vdb-dump -R $row_range -T PRIMARY_ALIGNMENT -C $col_list -f tab";
$varexpand="/panfs/traces01.be-md.ncbi.nlm.nih.gov/trace_software/toolkit/centos64/bin/var-expand.2.6.2 --algorithm ra";

#open OUT,"|$varexpand"     or die "failed to run $varexpand";
open DUMP,"$vdbdump $acc|" or die "failed to run $vdbdump";
open TMP_COVERAGE,"|sort -n| uniq > tmp_coverage";
while(<DUMP>)
{
        chop;
        ($spotid,$rid,$is_reversed,$ref,$pos,$cigar,$read,$rf)=split '\t';
        if($rf ne "SRA_READ_FILTER_PASS") {next;}
        flush_cache($pos);
        if($is_reversed eq "true"){ 
                if($rid == 1){
                        $ro='P-';
                } else {
                        $ro='P+';
                }
        } elsif($rid == 1){
                $ro='N+';
        } else {
                $ro='N-';
        }
        process_cigar();

}
close DUMP;
close TMP_COVERAGE;
flush_cache(0);


sub flush_cache
{
        my $pp=shift @_;
        foreach $p (sort keys %allele_cache)
        {
                if($pp==0 || $p < $pp ){
                        foreach $a (keys %{$allele_cache{$p}}){
                                print("$allele_cache{$p}{$a}\t$a\n");
                                #delete $allele_cache{$p}{$a};
                        }
                        if($pp!=0) {delete $allele_cache{$p};}
                }
        }
}

#close OUT
sub process_cigar
{
        @cig = split(/([=XSIDNHMP])/, $cigar);
        if($#cig > 1 || $cig[1]=~/[I=]/){
                $off=0;
                $allele="";
                $del=0;
                while(@cig){
                        $len=$cig[0];
                        $op =$cig[1];
                        shift @cig;shift@cig;
                        $next_op=$cig[1];
                        if(     $op eq '='){ #just consume
								for($i=0;$i<$len;$i++){
									printf TMP_COVERAGE "%d\t%d%s\n",$pos+$i,$spotid,$ro;
								}
                                $pos+=$len;
                                substr($read,0,$len)="";
						
                        } elsif ($op eq 'X'){
                                if( $next_op =~ /[=DIX]/){#don't report anything not bounded by real variation
                                        $off+=$len;
                                        $del+=$len;
                                        $allele.=substr($read,0,$len);
                                        substr($read,0,$len)="";
                                        if(  $next_op =~ /[DIX]/ ) { next;} #accumuilate adjacent variation
                                } else {
                                        $pos+=$del;
                                        substr($read,0,$len)="";
                                }
                        } elsif ($op eq 'S'){
                                substr($read,0,$len)="";
                        } elsif ($op eq 'D'){
                                if( $next_op =~ /[=DIX]/){#don't report anything not bounded by real variation
                                        $off+=$len;
                                        $del+=$len;
                                        if(  $next_op =~ /[DIX]/ ) { next;} #accumuilate adjacent variation
                                } else {
                                         $pos+=$del;
                                }
                        } elsif ($op eq 'I'){
                                if( $next_op =~ /[=DIX]/){#don't report anything not bounded by real variation
                                        $allele.=substr($read,0,$len);
                                        substr($read,0,$len)="";
                                        if(  $next_op =~ /[DIX]/ ) { next;} #accumuilate adjacent variation
                                } else {
                                        substr($read,0,$len)="";
                                }
                        } elsif ($op eq 'N'){ # accept and ignore
                                $pos+=$len
                        } else {
                                die "Unexpected Cigar: $cigar\n";
                        }
                        if($del !=0 || $allele ne ''){
                                if($allele eq '') { $allele='-';}
                                $signature="$ref:$pos:$del:$allele";
                                $allele_cache{$pos}{$signature}.="$spotid$ro";
                                #print "$spotid$ro\t$signature\n";
                                $pos+=$off;
                                $off=0;
                                $del=0;
                                $allele='';
                        }
                }
        }
}
