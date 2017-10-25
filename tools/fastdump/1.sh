acc="SRR341578"
scratch="-t /dev/shm/raetzw_tmp"
outfile="/dev/shm/raetzw_out/$acc.fastq"
outfile_0="/dev/shm/raetzw_out/$acc.fastq.0"
outfile_1="/dev/shm/raetzw_out/$acc.fastq.1"
out="-o $outfile"
numthreads="-e 6"
mem_limit="-m 50M"
cur_cache="-c 5M"
show_progress="-p"
force_overwrite="-f"
show_details="-x"
split_spot="-s"
split_file="-S"
rowid_as_name="-N"

# -t xxx    ... path for temp. files
# -o xxx    ... output-file
# -m n      ... memory-limit for merge-sort
# -c n      ... cursor-cache per thread

md5="66b97c245a2c605a8854967b454d789e"
md5_1="087c5292b808e2d30dfd3d9e08ba3c56"
md5_2="38c926a3c56ee3833ddd2092cacd0129"

fastdump_db_split_file_row_id_as_name()
{
    echo "***** fastdump database split into files with rowid as name *****"
    rm -rf $outfile.1 $outfile.2

    options=""
    options="$options $show_progress"
    options="$options $force_overwrite"
    options="$options $show_details"
    options="$options $split_spot"
    options="$options $split_file"
    options="$options $rowid_as_name"

    time fastdump $acc $options $numthreads $mem_limit $cur_cache $scratch $out
    echo "."

    md5sum $outfile.1 | awk '{print $1}'
    echo $md5_1

    echo "."

    md5sum $outfile.2 | awk '{print $1}'
    echo $md5_2
}

fastdump_db_split_file()
{
    echo "***** fastdump database split into files *****"
    rm -rf $outfile.1 $outfile.2

    options=""
    options="$options $show_progress"
    options="$options $force_overwrite"
    options="$options $show_details"
    options="$options $split_spot"
    options="$options $split_file"

    time fastdump $acc $options $numthreads $mem_limit $cur_cache $scratch $out
    echo "."

    md5sum $outfile.1 | awk '{print $1}'
    echo $md5_1

    echo "."

    md5sum $outfile.2 | awk '{print $1}'
    echo $md5_2
}

fastdump_db_split_row_id_as_name()
{
    echo "***** fastdump database split with rowid as name *****"
    rm -rf $outfile

    options=""
    options="$options $show_progress"
    options="$options $force_overwrite"
    options="$options $show_details"
    options="$options $split_spot"
    options="$options $rowid_as_name"
    
    time fastdump $acc $options $numthreads $mem_limit $cur_cache $scratch $out
    echo "."

    md5sum $outfile | awk '{print $1}'
    echo $md5
}

fastdump_db_split()
{
    echo "***** fastdump database split *****"
    rm -rf $outfile

    options=""
    options="$options $show_progress"
    options="$options $force_overwrite"
    options="$options $show_details"
    options="$options $split_spot"
    
    time fastdump $acc $options $numthreads $mem_limit $cur_cache $scratch $out
    echo "."

    md5sum $outfile | awk '{print $1}'
    echo $md5
}

fastdump_db_split_file_row_id_as_name
fastdump_db_split_file
fastdump_db_split_row_id_as_name
fastdump_db_split

#totalview fastdump -a $acc -p -f -x -s $numthreads $mem_limit $cur_cache $scratch $out
#md5sum $outfile | awk '{print $1}'

#rm -rf $outfile
