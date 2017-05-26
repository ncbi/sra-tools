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

# only 298,179 alignments
ACC5=SRR5490844

# only 492,201 alignments
ACC6=SRR5490843

# only 996,817
ACC7=ERR111049

# 817,426,459 alignments
#ACC5=SRR3986881

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
    #FILTER="--min-t+ 8 --min-t- 7"
    FILTER=""
    
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

function sam_events_vs_perl_scripts
{
    ACC=$1
    SAM_EVENTS_OUT="$ACC.SAM_EVENTS.txt"
    PERL_SCRIPTS_OUT="$ACC.PERL_SCRIPTS.txt"
    COLLECT_OUT="$ACC.COLLECTED.txt"
    
    time sam-events $ACC --csra --min-each 1 --min-any 4 > $SAM_EVENTS_OUT
    
    GET_ALLELES="./NewPileup/get_alleles_new.pl $ACC"
    PROCESS_EXPANDED="./NewPileup/process_expanded_alleles_new.pl"
    #time $GET_ALLELES | tee $ACC.before.txt | var-expand | tee $ACC.after.txt | $PROCESS_EXPANDED | ./transform.py > $PERL_SCRIPTS_OUT
    time $GET_ALLELES |  var-expand --algorithm ra | $PROCESS_EXPANDED | ./transform.py > $PERL_SCRIPTS_OUT
    
    ./collect2.py $SAM_EVENTS_OUT $PERL_SCRIPTS_OUT | sort | ./filter_equals.py > $COLLECT_OUT
    rm -rf $SAM_EVENTS_OUT $PERL_SCRIPTS_OUT
}

function inspect_t4_vs_perl_script_v1
{
    ACC=$1
    T4_OUT="$ACC.T4.txt"
    GETA_OUT="$ACC.GETA.txt"

    time t4 $ACC -e | sort > $T4_OUT
    
    GET_ALLELES="./NewPileup/get_alleles_new.pl $ACC"
    time $GET_ALLELES | ./strip_phase.py | sort > $GETA_OUT
    diff2 $T4_OUT $GETA_OUT
}

function inspect_t4_vs_perl_script_v2
{
    ACC=$1
    T4_OUT="$ACC.T4.txt"
    GETA_OUT="$ACC.GETA.txt"
    #ROWS="142746"
    
    #time t4 $ACC -R $ROWS -l | sort > $T4_OUT
    time t4 $ACC -l | sort > $T4_OUT
    
    #GET_ALLELES="./NewPileup/get_alleles_new.pl $ACC $ROWS"
    GET_ALLELES="./NewPileup/get_alleles_new.pl $ACC"
    VAR_EXP="/home/raetzw/devel/ncbi/ngs-tools/build/cmake/Debug/tools/ref-variation/var-expand --algorithm ra"
    
    time $GET_ALLELES | $VAR_EXP | ./strip_phase2.py | sort > $GETA_OUT
    diff2 $T4_OUT $GETA_OUT
}

function debug_var_expand
{
    ACC=$1
    ROW="142746"
    TEMP1="$ACC.GETA_1.txt"
    
    GET_ALLELES="./NewPileup/get_alleles_new.pl $ACC $ROW"
    VAR_EXP="/home/raetzw/devel/ncbi/ngs-tools/build/cmake/Debug/tools/ref-variation/var-expand"
    $GET_ALLELES > $TEMP1
    totalview $VAR_EXP
    time $GET_ALLELES | $VAR_EXP | ./strip_phase2.py | sort > $GETA_OUT
}

#prepare $ACC2
#test2ways $ACC4

#run_sra_vs_sam $ACC4

#run_with_lookup $ACC4
#time sam-events $ACC4 --csra --lookup $LMDB_CACHE

#time sam-events $ACC4 --csra > .txt
#time sam-events $ACC4 --csra --evstrat 1 > 2.txt
#diff2 1.txt 2.txt

sam_events_vs_perl_scripts $ACC7
#inspect_t4_vs_perl_script_v2 $ACC5
