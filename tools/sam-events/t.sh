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

    execute "sam-events $ACC.sam --reference $ACC.fasta --fast --reduce" "$ACC.fast.events"

    execute "sam-events $ACC.sam --reference $ACC.fasta --reduce" "$ACC.slow.events"

    execute "sam-events ./$ACC.sorted --reduce --csra" "$ACC.csra.events"

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

function run_on_big_acc
{
    ACC=$1
    
    #get the reference as fasta-file out of the accession
    execute "vdb-dump $ACC -T REFERENCE -f fasta2" "$ACC.fasta"
    
    #pipe the output of sam-dump into the tool
    execute "( sam-dump $ACC --seqid -n -1 ) | sam-events --reference $ACC.fasta --fast --reduce" "$ACC.fast.events"

    #get the events directly out of the accession
    execute "sam-events $ACC --csra --reduce" "$ACC.csra.events"

    #diff2 $ACC.fast.events $ACC.csra.events
    diff_sorted $ACC.fast.events $ACC.csra.events
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

#run_on_big_acc $ACC4.sorted
run_on_big_acc $ACC4

#-----------------------------------------------------------------------
#                      test for memory leaks
#-----------------------------------------------------------------------
#valgrind --ncbi --leak-check=full --log-file=vg.log sam-events 1.sam --reference CM000459_1.fasta --limit 10000 --reduce > 1.txt
#valgrind --orig --tool=callgrind --log-file=vg1.log sam-events 1.sam --reference CM000459_1.fasta --limit 10000 --reduce > 1.txt

#sam-events 1.sam --reference CM000459_1.fasta --limit 100000 --purge 2048 --reduce > 1.txt
#sam-events 1.sam --reference CM000459_1.fasta --reduce > 1.txt

#cat 1.sam | sam-events --reference CM000459_1.fasta --limit 100000 --purge 2048 --reduce > 1.txt
#cat 1.sam | sam-events --reference CM000459_1.fasta --reduce > 2.txt

#-----------------------------------------------------------------------
#                      compare fasts vs validated
#-----------------------------------------------------------------------
#echo "not validated SAM:"
#time sam-events 1.sam --reference CM000459_1.fasta --reduce --fast > 2.txt

#echo "validated SAM:"
#time sam-events 1.sam --reference CM000459_1.fasta --reduce > 1.txt

#diff -qs 1.txt 2.txt

#-----------------------------------------------------------------------
#                      get events from sra-accession
#-----------------------------------------------------------------------
#time sam-events $ACC --reduce --csra > 3.txt
