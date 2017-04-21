#-----------------------------------------------------------------------
#            dump just one relatively small chromosome
#-----------------------------------------------------------------------
#sam-dump SRR834507 --seqid --aligned-region CM000459.1 > 1.sam

# in order to build we need branch VDB-3307 build at ncbi-vdb

ACC1=SRR834507
ACC2=SRR5323913
ACC3=SRR5330399
ACC4=ERR1881852
ACC5=SRR3986881

LMDB_CACHE=/panfs/traces01/sra_review/scratch/raetzw/lmdb_cache

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

    diff2 $ACC.fast.events $ACC.csra.events
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

function run_with_lookup
{
    ACC=$1
    execute "sam-events $ACC --csra -m 4 --lookup $LMDB_CACHE" "$ACC.looked.up"
}

#prepare $ACC2
#test3ways $ACC2

run_sra_vs_sam $ACC4

#run_with_lookup $ACC4
#sam-events $ACC4 --csra --lookup $LMDB_CACHE
#sam-events $ACC4 --csra

a1="NC_000001.10:10055:5:ACCCC"
a2="NC_000001.10:16494:2:CG"
#alookup -c $LMDB_CACHE $a1 $a2
#time alookup -c $LMDB_CACHE > c.txt

