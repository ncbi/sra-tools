acc="SRR341578"
numthreads="6"
scratch="/dev/shm/raetzw_tmp"
out="/dev/shm/raetzw_out/$acc.fastq"
mem_limit="2G"

# -p ... show progress-bar
# -f ... force to overwrite output-file
# -x ... print details of actions
# -s ... split fastq
# -e n      ... number of threads to use
# -t xxx    ... path for temp. files
# -o xxx    ... output-file
# -m n      ... memory-limit for merge-sort

md5="66b97c245a2c605a8854967b454d789e"

rm -rf $out
time fastdump $acc -p -f -x -s -e $numthreads -t $scratch -o $out -m $mem_limit
#totalview fastdump -a $acc -e $numthreads -x -p --split -t $scratch -o $out -f
md5sum $out
echo $md5
