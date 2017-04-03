#-----------------------------------------------------------------------
#            dump just one relatively small chromosome
#-----------------------------------------------------------------------
#sam-dump SRR834507 --seqid --aligned-region CM000459.1 > 1.sam

# in order to build we need branch VDB-3307 build at ncbi-vdb

ACC1=SRR834507
ACC2=SRR5323913
ACC3=SRR5330399
ACC4=ERR1881852


function execute
{
    CMD=$1
    OUTFILE=$2
    echo .
    echo "$CMD > $OUTFILE"
    eval $CMD | pv > $OUTFILE
}


function test3ways
{
    ACC=$1

    execute "sam-events $ACC.sam --reference $ACC.fasta --fast" "$ACC.fast.events"

    execute "sam-events $ACC.sam --reference $ACC.fasta" "$ACC.slow.events"

    execute "sam-events ./$ACC.sorted --csra" "$ACC.csra.events"

    diff -qs $ACC.fast.events $ACC.csra.events
    diff -qs $ACC.fast.events $ACC.slow.events
}

function diff2
{
    diff -qs $1 $2
}

function diff_sorted
{
    sort $1 > $1.sorted
    sort $2 > $2.sorted
    diff2 $1.sorted $2.sorted
}

function run_sra_vs_sam
{
    ACC=$1
    FILTER="--min-t+ 8 --min-t- 7"
    
    #get the reference as fasta-file out of the accession
    execute "vdb-dump $ACC -T REFERENCE -f fasta2" "$ACC.fasta"
    
    #pipe the output of sam-dump into the tool
    execute "( sam-dump $ACC --no-mate-cache --seqid -n -u -1 ) | sam-events --reference $ACC.fasta $FILTER" "$ACC.fast.events"

    #get the events directly out of the accession
    execute "sam-events $ACC --csra $FILTER" "$ACC.csra.events"

    #diff2 $ACC.fast.events $ACC.csra.events
    diff_sorted $ACC.fast.events $ACC.csra.events
}


function run_checked_vs_unchecked
{
    ACC=$1

    #get the reference as fasta-file out of the accession
    execute "vdb-dump $ACC -T REFERENCE -f fasta2" "$ACC.fasta"

    #produce a sam-file from the accession
    execute "sam-dump $ACC --no-mate-cache --seqid -n -u -1" "$ACC.sam"
    
    #pipe the output of sam-dump into the tool ( unchecked == --fast )
    execute "sam-events $ACC.sam --reference $ACC.fasta --fast" "$ACC.unchecked.events"
    
    #pipe the output of sam-dump into the tool ( checked )
    execute "sam-events $ACC.sam --reference $ACC.fasta" "$ACC.checked.events"

    diff2 $ACC.unchecked.events $ACC.checked.events
}

function sort_run_on_big_acc
{
    ACC=$1
    
    #sort the accession
    echo "sorting $ACC"
    rm -rf $ACC.sorted
    time sra-sort $ACC $ACC.sorted
    
    #run on sorted accession
    run_on_big_acc $ACC.sorted
}

#prepare $ACC2
#test3ways $ACC2

#run_sra_vs_sam $ACC4
run_checked_vs_unchecked $ACC4
