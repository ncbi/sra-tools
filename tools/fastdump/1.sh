scratch="-t /dev/shm/raetzw_tmp"
numthreads="-e 6"
mem_limit="-m 50M"
cur_cache="-c 5M"
show_progress="-p"
force_overwrite="-f"
show_details="-x"
split_spot="-s"
split_file="-S"
rowid_as_name="-N"

# $1...file1, $2...md5
compare_md5()
{
    sum1=`md5sum $1 | awk '{print $1}'`
    if [ "$sum1" == "$2" ]
    then
        echo "$1 ... OK"
    else
        echo "$1 ... ERR ( $sum1 != $2 )"
    fi
}

fastdump_split_file_row_id_as_name()
{
    echo "***** fastdump database split into files with rowid as name *****"
    out="/dev/shm/raetzw_out/$1.fastq"

    rm -rf $out.1 $out.2

    options="$show_progress $force_overwrite $show_details $split_spot $split_file $rowid_as_name"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out.1 $2
    compare_md5 $out.2 $3
}

fastdump_split_file()
{
    echo "***** fastdump database split into files *****"
    out="/dev/shm/raetzw_out/$1.fastq"
    
    rm -rf $out.1 $out.2

    options="$show_progress $force_overwrite $show_details $split_spot $split_file"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out.1 $2
    compare_md5 $out.2 $3
}

fastdump_split_row_id_as_name()
{
    echo "***** fastdump database split with rowid as name *****"
    out="/dev/shm/raetzw_out/$1.fastq"
    
    rm -rf $out

    options="$show_progress $force_overwrite $show_details $split_spot $rowid_as_name"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

fastdump_split()
{
    echo "***** fastdump database split *****"
    out="/dev/shm/raetzw_out/$1.fastq"
   
    rm -rf $out

    options="$show_progress $force_overwrite $show_details $split_spot"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    #totalview fastdump -a $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

fastdump_not_split_row_id_as_name()
{
    echo "***** fastdump database not split with rowid as name *****"
    out="/dev/shm/raetzw_out/$1.fastq"
   
    rm -rf $out

    options="$show_progress $force_overwrite $show_details $rowid_as_name"    
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

fastdump_not_split()
{
    echo "***** fastdump database not split *****"
    out="/dev/shm/raetzw_out/$1.fastq"
   
    rm -rf $out

    options="$show_progress $force_overwrite $show_details"
    time fastdump $1 $options $numthreads $mem_limit $cur_cache $scratch -o $out
    compare_md5 $out $2
}

acc_db="SRR341578"

#none-splitted
md5_db_n="b6832cde19b4083ea0a5a246bad1a670"
#splitted
md5_db_s="66b97c245a2c605a8854967b454d789e"
md5_db_1="087c5292b808e2d30dfd3d9e08ba3c56"
md5_db_2="38c926a3c56ee3833ddd2092cacd0129"

#fastdump_split_file_row_id_as_name $acc_db $md5_db_1 $md5_db_2
#fastdump_split_file $acc_db $md5_db_1 $md5_db_2
#fastdump_split_row_id_as_name $acc_db $md5_db_s
#fastdump_split $acc_db $md5_db_s
#fastdump_not_split_row_id_as_name $acc_db $md5_db_n
#fastdump_not_split $acc_db $md5_db_n

#acc_tbl="SRR072810"
#none-splitted
#md5_tbl_n="4685a80588fed68450a6da093be72e4a"

acc_tbl="SRR942391"

#none-splitted
md5_tbl_n="590366f579aa503bbedec1dab66df2ad"
#splitted
md5_tbl_s="a668be91b3e57b11fc56b977af1c1426"
md5_tbl_1="fff51585bf9963419e4ce505c0f57637"
md5_tbl_2="20784e715bd2498aca0e82ec61a195b9"

fastdump_split_file $acc_tbl $md5_tbl_1 $md5_tbl_2
#fastdump_split $acc_tbl $md5_tbl_s
#fastdump_not_split_row_id_as_name $acc_tbl $md5_tbl_n
#fastdump_not_split $acc_tbl $md5_tbl_n
