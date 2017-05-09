#-----------------------------------------------------------------------
#            dump just one relatively small chromosome
#-----------------------------------------------------------------------
#sam-dump SRR834507 --seqid --aligned-region CM000459.1 > 1.sam

# in order to build we need branch VDB-3307 build at ncbi-vdb

# 34,515,481 alignments
ACC1=SRR834507

# only 76,824 alignments ( slightly unsorted! )
ACC2=SRR5323913

# only 191,092 alignments ( slightly unsorted! )
ACC3=SRR5330399

# only 177,968 alignments ( sorted )
ACC4=ERR1881852

# MAL1(NC_004325.1) 643,292     MAL2(NC_000910.2) 947,102       MAL3(NC_000521.3) 1,060,087
# MAL4(NC_004318.1) 1,204,112   MAL5(NC_004326.1) 1,343,552     MAL6(NC_004327.2) 1,418,244
# MAL7(NC_004328.2) 1,501,717   MAL8(NC_004329.2) 1,419,563     MAL9(NC_004330.1) 1,541,723
# MAL10(NC_004314.2) 1,687,655  MAL11(NC_004315.2) 2,038,337    MAL12(NC_004316.1) 2,271,477
# MAL13(NC_004331.2) 2,895,605  MAL14(NC_004317.2) 3,291,871

# 817,426,459 alignments
ACC5=SRR3986881

# a copy of the lmdb-cache for the dbSNP team
LMDB_CACHE=/panfs/traces01/sra_review/scratch/raetzw/lmdb_cache

function execute
{
    CMD=$1
    OUTFILE=$2
    echo .
    echo "$CMD > $OUTFILE"
    eval $CMD | pv > $OUTFILE
}


function test2ways
{
    ACC=$1

    execute "sam-events $ACC.sam --reference $ACC.fasta" "$ACC.fast.events"
    execute "sam-events ./$ACC.sorted --csra" "$ACC.csra.events"

    diff -qs $ACC.fast.events $ACC.csra.events
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
#test2ways $ACC2

#run_sra_vs_sam $ACC4

#run_with_lookup $ACC4
#time sam-events $ACC4 --csra --lookup $LMDB_CACHE

#time t2 $ACC4 > 1.txt

#time sam-events $ACC4 --csra > 1.txt
time sam-events $ACC1 --csra --strat 1 > 2.txt
#diff2 1.txt 2.txt

#valgrind --ncbi sam-events $ACC4 --csra --strat 1 > 1.txt 2>vg.txt
